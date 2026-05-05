#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "src/sem.h"

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(expr) do {                                                   \
    tests_run++;                                                           \
    if (!(expr)) {                                                         \
        tests_failed++;                                                    \
        fprintf(stderr, "FAIL  line %d: %s\n", __LINE__, #expr);           \
    }                                                                      \
} while (0)

#define CHECK_EQ_U(actual, expected) do {                                  \
    tests_run++;                                                           \
    unsigned long _a = (unsigned long)(actual);                            \
    unsigned long _e = (unsigned long)(expected);                          \
    if (_a != _e) {                                                        \
        tests_failed++;                                                    \
        fprintf(stderr,                                                    \
                "FAIL  line %d: %s == %s  (got 0x%lx, expected 0x%lx)\n",  \
                __LINE__, #actual, #expected, _a, _e);                     \
    }                                                                      \
} while (0)

/* -------- Mock CS primitives --------
 *
 * These stand in for the real cs_enter/cs_exit (which on a Cortex-M
 * would touch PRIMASK). Tests inspect these counters to verify that:
 *   - every public function that does an RMW actually entered a CS,
 *   - every CS entered was exited (no early-return leaks),
 *   - depth is 0 on every public-function return.
 */
static int cs_depth = 0;
static int cs_enter_count = 0;
static int cs_exit_count = 0;

void cs_enter(void) {
    cs_depth++;
    cs_enter_count++;
}

void cs_exit(void) {
    cs_depth--;
    cs_exit_count++;
}

static void cs_reset(void) {
    cs_depth = 0;
    cs_enter_count = 0;
    cs_exit_count = 0;
}

/* Snapshot the CS counters before and after an operation, and assert
 * the depth returned to 0 and at least one enter/exit pair happened. */
#define CHECK_CS_BALANCED_AT_LEAST_ONCE() do { \
    CHECK_EQ_U(cs_depth, 0);                   \
    CHECK(cs_enter_count >= 1);                \
    CHECK_EQ_U(cs_enter_count, cs_exit_count); \
} while (0)

static void test_init_clamps_initial(void) {
    sem_t s;
    cs_reset();

    sem_init(&s, 10, 4);                /* initial > max_count */
    CHECK_EQ_U(s.max_count, 4);
    CHECK_EQ_U(s.count, 4);             /* clamped */

    sem_init(&s, 2, 4);                 /* normal case */
    CHECK_EQ_U(s.max_count, 4);
    CHECK_EQ_U(s.count, 2);

    sem_init(&s, 0, 4);                 /* empty */
    CHECK_EQ_U(s.count, 0);
}

static void test_post_saturates_at_max(void) {
    sem_t s;
    sem_init(&s, 0, 3);

    cs_reset();
    CHECK(sem_post(&s) == true);
    CHECK_CS_BALANCED_AT_LEAST_ONCE();
    CHECK_EQ_U(s.count, 1);

    CHECK(sem_post(&s) == true);
    CHECK_EQ_U(s.count, 2);
    CHECK(sem_post(&s) == true);
    CHECK_EQ_U(s.count, 3);

    /* At cap: must return false and leave count alone. */
    cs_reset();
    CHECK(sem_post(&s) == false);
    CHECK_CS_BALANCED_AT_LEAST_ONCE();   /* even the failed path locks */
    CHECK_EQ_U(s.count, 3);
}

static void test_try_take_drains_to_zero(void) {
    sem_t s;
    sem_init(&s, 2, 4);

    cs_reset();
    CHECK(sem_try_take(&s) == true);
    CHECK_CS_BALANCED_AT_LEAST_ONCE();
    CHECK_EQ_U(s.count, 1);

    CHECK(sem_try_take(&s) == true);
    CHECK_EQ_U(s.count, 0);

    /* Empty: must return false and leave count alone. */
    cs_reset();
    CHECK(sem_try_take(&s) == false);
    CHECK_CS_BALANCED_AT_LEAST_ONCE();
    CHECK_EQ_U(s.count, 0);
}

static void test_try_take_n_all_or_nothing(void) {
    sem_t s;

    /* Exactly enough: succeed, count goes to 0. */
    sem_init(&s, 5, 10);
    cs_reset();
    CHECK(sem_try_take_n(&s, 5) == true);
    CHECK_CS_BALANCED_AT_LEAST_ONCE();
    CHECK_EQ_U(s.count, 0);

    /* Plenty: succeed. */
    sem_init(&s, 5, 10);
    CHECK(sem_try_take_n(&s, 3) == true);
    CHECK_EQ_U(s.count, 2);

    /* Under-supply: must NOT partial-decrement, must return false,
     * count unchanged. */
    sem_init(&s, 2, 10);
    cs_reset();
    CHECK(sem_try_take_n(&s, 5) == false);
    CHECK_CS_BALANCED_AT_LEAST_ONCE();
    CHECK_EQ_U(s.count, 2);              /* untouched */

    /* Take-zero edge case: should be a no-op success, count unchanged. */
    sem_init(&s, 4, 10);
    CHECK(sem_try_take_n(&s, 0) == true);
    CHECK_EQ_U(s.count, 4);
}

static void test_count_reads_under_cs(void) {
    sem_t s;
    sem_init(&s, 7, 10);

    cs_reset();
    uint16_t c = sem_count(&s);
    CHECK_EQ_U(c, 7);
    CHECK_CS_BALANCED_AT_LEAST_ONCE();

    /* And matches the field after a couple of ops. */
    (void)sem_try_take(&s);
    (void)sem_post(&s);
    (void)sem_post(&s);
    CHECK_EQ_U(sem_count(&s), 8);
}

static void test_post_take_roundtrip(void) {
    sem_t s;
    sem_init(&s, 0, 5);

    /* Alternate post/take across many cycles; count never exceeds 5,
     * never goes negative. */
    for (int i = 0; i < 20; i++) {
        CHECK(sem_post(&s) == true);
        CHECK(sem_try_take(&s) == true);
        CHECK_EQ_U(sem_count(&s), 0);
    }

    /* Fill, then drain. */
    for (int i = 0; i < 5; i++) CHECK(sem_post(&s) == true);
    CHECK_EQ_U(sem_count(&s), 5);
    CHECK(sem_post(&s) == false);   /* at cap */
    for (int i = 0; i < 5; i++) CHECK(sem_try_take(&s) == true);
    CHECK_EQ_U(sem_count(&s), 0);
    CHECK(sem_try_take(&s) == false); /* empty */
}

/* If the candidate forgets cs_exit on the failure path of one of the
 * conditional ops, the global cs_depth will leak across calls. This
 * test does many mixed operations and asserts depth returns to 0
 * after each one. */
static void test_no_cs_leak_on_failure_paths(void) {
    sem_t s;
    sem_init(&s, 0, 2);
    cs_reset();

    /* Failing post (already empty, then fill and try one more). */
    (void)sem_post(&s);
    (void)sem_post(&s);
    CHECK_EQ_U(cs_depth, 0);
    (void)sem_post(&s);          /* fails, at cap */
    CHECK_EQ_U(cs_depth, 0);

    /* Failing try_take. */
    (void)sem_try_take(&s);
    (void)sem_try_take(&s);
    (void)sem_try_take(&s);      /* fails, empty */
    CHECK_EQ_U(cs_depth, 0);

    /* Failing try_take_n. */
    (void)sem_post(&s);
    (void)sem_try_take_n(&s, 5); /* fails */
    CHECK_EQ_U(cs_depth, 0);

    /* And the global counters balance. */
    CHECK_EQ_U(cs_enter_count, cs_exit_count);
}

int main(void) {
    test_init_clamps_initial();
    test_post_saturates_at_max();
    test_try_take_drains_to_zero();
    test_try_take_n_all_or_nothing();
    test_count_reads_under_cs();
    test_post_take_roundtrip();
    test_no_cs_leak_on_failure_paths();

    int passed = tests_run - tests_failed;
    printf("\n%d/%d tests passed\n", passed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
