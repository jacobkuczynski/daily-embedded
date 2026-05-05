#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "src/scheduler.h"

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

/* ---- Mock critical-section primitives --------------------------------- */
static int g_critical_depth = 0;
static int g_critical_enter_count = 0;

void critical_enter(void) {
    g_critical_depth++;
    g_critical_enter_count++;
}
void critical_exit(void) {
    g_critical_depth--;
}

static void reset_critical_counters(void) {
    g_critical_depth = 0;
    g_critical_enter_count = 0;
}

/* ---- Callback bookkeeping --------------------------------------------- */
static int  cb_a_count = 0;
static int  cb_b_count = 0;
static void *cb_last_ctx = NULL;

static void cb_a(void *ctx) { cb_a_count++; cb_last_ctx = ctx; }
static void cb_b(void *ctx) { cb_b_count++; cb_last_ctx = ctx; }

static void reset_cb_counters(void) {
    cb_a_count = 0;
    cb_b_count = 0;
    cb_last_ctx = NULL;
}

/* Tick the ISR n times. */
static void tick(scheduler_t *s, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        scheduler_tick_isr(s);
    }
}

/* ---- Tests ------------------------------------------------------------ */

static void test_init(void) {
    scheduler_t s;
    scheduler_init(&s);
    reset_critical_counters();
    reset_cb_counters();

    CHECK_EQ_U(scheduler_now(&s), 0);
    CHECK_EQ_U(scheduler_dispatch(&s), 0);
    CHECK_EQ_U(g_critical_depth, 0);
}

static void test_schedule_and_fire_at_deadline(void) {
    scheduler_t s;
    scheduler_init(&s);
    reset_critical_counters();
    reset_cb_counters();

    int tag = 42;
    CHECK(scheduler_schedule(&s, 5, cb_a, &tag) == true);

    /* Before the deadline: nothing fires. */
    tick(&s, 4);
    CHECK_EQ_U(scheduler_dispatch(&s), 0);
    CHECK_EQ_U(cb_a_count, 0);

    /* At the deadline: fires exactly once. */
    tick(&s, 1);
    CHECK_EQ_U(scheduler_dispatch(&s), 1);
    CHECK_EQ_U(cb_a_count, 1);
    CHECK(cb_last_ctx == &tag);
}

static void test_no_double_fire(void) {
    scheduler_t s;
    scheduler_init(&s);
    reset_critical_counters();
    reset_cb_counters();

    CHECK(scheduler_schedule(&s, 1, cb_a, NULL) == true);
    tick(&s, 5);

    CHECK_EQ_U(scheduler_dispatch(&s), 1);
    CHECK_EQ_U(scheduler_dispatch(&s), 0);   /* must NOT fire again */
    CHECK_EQ_U(cb_a_count, 1);
}

static void test_dispatch_before_deadline_no_fire(void) {
    scheduler_t s;
    scheduler_init(&s);
    reset_critical_counters();
    reset_cb_counters();

    CHECK(scheduler_schedule(&s, 100, cb_a, NULL) == true);
    tick(&s, 99);
    CHECK_EQ_U(scheduler_dispatch(&s), 0);
    CHECK_EQ_U(cb_a_count, 0);
}

static void test_multiple_timers_all_due(void) {
    scheduler_t s;
    scheduler_init(&s);
    reset_critical_counters();
    reset_cb_counters();

    CHECK(scheduler_schedule(&s, 1, cb_a, NULL) == true);
    CHECK(scheduler_schedule(&s, 2, cb_b, NULL) == true);
    CHECK(scheduler_schedule(&s, 3, cb_a, NULL) == true);

    tick(&s, 10);
    CHECK_EQ_U(scheduler_dispatch(&s), 3);
    CHECK_EQ_U(cb_a_count, 2);
    CHECK_EQ_U(cb_b_count, 1);
}

static void test_table_full_returns_false(void) {
    scheduler_t s;
    scheduler_init(&s);
    reset_critical_counters();
    reset_cb_counters();

    for (uint8_t i = 0; i < SCHED_MAX_TIMERS; i++) {
        CHECK(scheduler_schedule(&s, 100, cb_a, NULL) == true);
    }
    /* The next one must fail; nothing fires yet because we haven't ticked. */
    CHECK(scheduler_schedule(&s, 100, cb_a, NULL) == false);
    CHECK_EQ_U(cb_a_count, 0);
}

static void test_null_callback_rejected(void) {
    scheduler_t s;
    scheduler_init(&s);
    reset_critical_counters();
    reset_cb_counters();

    CHECK(scheduler_schedule(&s, 1, NULL, NULL) == false);
    tick(&s, 5);
    CHECK_EQ_U(scheduler_dispatch(&s), 0);
}

static void test_cancel_before_fire(void) {
    scheduler_t s;
    scheduler_init(&s);
    reset_critical_counters();
    reset_cb_counters();

    int x = 1, y = 2;
    CHECK(scheduler_schedule(&s, 5, cb_a, &x) == true);
    CHECK(scheduler_schedule(&s, 5, cb_a, &y) == true);

    CHECK(scheduler_cancel(&s, cb_a, &x) == true);
    /* Cancelling the same one twice must report false the second time. */
    CHECK(scheduler_cancel(&s, cb_a, &x) == false);

    tick(&s, 10);
    CHECK_EQ_U(scheduler_dispatch(&s), 1);
    CHECK_EQ_U(cb_a_count, 1);
    CHECK(cb_last_ctx == &y);
}

static void test_cancel_unknown_returns_false(void) {
    scheduler_t s;
    scheduler_init(&s);
    reset_critical_counters();
    reset_cb_counters();

    int z = 0;
    CHECK(scheduler_cancel(&s, cb_a, &z) == false);
}

/* Reentrant: callback re-schedules itself. The crux is whether the
 * scheduler frees the slot BEFORE invoking the callback. If it doesn't,
 * the table looks full from inside the callback and the re-schedule
 * fails — the test catches this by counting how many times cb_self
 * actually runs. */
static scheduler_t *g_re_sched = NULL;
static int          g_re_count = 0;
static void cb_self_reschedule(void *ctx) {
    (void)ctx;
    g_re_count++;
    if (g_re_count < 3) {
        /* Re-arm for one tick from now. */
        bool ok = scheduler_schedule(g_re_sched, 1, cb_self_reschedule, NULL);
        CHECK(ok == true);
    }
}

static void test_callback_can_reschedule(void) {
    scheduler_t s;
    scheduler_init(&s);
    reset_critical_counters();
    reset_cb_counters();

    g_re_sched = &s;
    g_re_count = 0;

    /* Fill all but one slot with far-future cb_a timers, so the
     * self-rescheduling timer has to take the same slot it was just
     * dispatched from. */
    for (uint8_t i = 0; i < SCHED_MAX_TIMERS - 1U; i++) {
        CHECK(scheduler_schedule(&s, 1000, cb_a, NULL) == true);
    }
    CHECK(scheduler_schedule(&s, 1, cb_self_reschedule, NULL) == true);

    /* Each tick advances one ms; each dispatch fires the self-rescheduler
     * once and re-arms it for the next ms. After three ticks we should
     * have run cb_self_reschedule three times. */
    tick(&s, 1);
    CHECK_EQ_U(scheduler_dispatch(&s), 1);
    tick(&s, 1);
    CHECK_EQ_U(scheduler_dispatch(&s), 1);
    tick(&s, 1);
    CHECK_EQ_U(scheduler_dispatch(&s), 1);

    CHECK_EQ_U(g_re_count, 3);
    CHECK_EQ_U(cb_a_count, 0);   /* cb_a is way in the future */
}

/* A delay_ms == 0 timer scheduled INSIDE a callback must not fire on the
 * same dispatch pass that ran the parent. Otherwise a misbehaving callback
 * can starve the main loop. */
static int g_zero_delay_inner_count = 0;
static void cb_zero_delay_inner(void *ctx) {
    (void)ctx;
    g_zero_delay_inner_count++;
}
static void cb_schedules_zero_delay(void *ctx) {
    scheduler_t *s = (scheduler_t *)ctx;
    bool ok = scheduler_schedule(s, 0, cb_zero_delay_inner, NULL);
    CHECK(ok == true);
}

static void test_zero_delay_inside_callback_defers_to_next_dispatch(void) {
    scheduler_t s;
    scheduler_init(&s);
    reset_critical_counters();
    reset_cb_counters();
    g_zero_delay_inner_count = 0;

    CHECK(scheduler_schedule(&s, 0, cb_schedules_zero_delay, &s) == true);

    /* Dispatch at now == 0: outer fires; inner is freshly scheduled but
     * MUST NOT fire on this same pass. */
    CHECK_EQ_U(scheduler_dispatch(&s), 1);
    CHECK_EQ_U(g_zero_delay_inner_count, 0);

    /* Next dispatch — inner is due (deadline 0 <= now 0) and fires. */
    CHECK_EQ_U(scheduler_dispatch(&s), 1);
    CHECK_EQ_U(g_zero_delay_inner_count, 1);
}

static void test_critical_section_balanced_and_used(void) {
    scheduler_t s;
    scheduler_init(&s);
    reset_critical_counters();
    reset_cb_counters();

    /* scheduler_now must wrap the 32-bit read in a critical section. */
    (void)scheduler_now(&s);
    CHECK_EQ_U(g_critical_depth, 0);
    CHECK(g_critical_enter_count >= 1);

    int saved = g_critical_enter_count;

    /* scheduler_dispatch must do the same when reading now_ms. */
    (void)scheduler_dispatch(&s);
    CHECK_EQ_U(g_critical_depth, 0);
    CHECK(g_critical_enter_count > saved);
}

int main(void) {
    test_init();
    test_schedule_and_fire_at_deadline();
    test_no_double_fire();
    test_dispatch_before_deadline_no_fire();
    test_multiple_timers_all_due();
    test_table_full_returns_false();
    test_null_callback_rejected();
    test_cancel_before_fire();
    test_cancel_unknown_returns_false();
    test_callback_can_reschedule();
    test_zero_delay_inside_callback_defers_to_next_dispatch();
    test_critical_section_balanced_and_used();

    int passed = tests_run - tests_failed;
    printf("\n%d/%d tests passed\n", passed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
