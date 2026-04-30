#include <stdint.h>
#include <stdio.h>
#include "src/ringbuffer.h"

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

static void test_init_and_empty(void) {
    ring_buffer_t rb;
    rb_init(&rb);
    CHECK(rb_is_empty(&rb) == true);
    CHECK(rb_is_full(&rb) == false);
    CHECK_EQ_U(rb.head, 0);
    CHECK_EQ_U(rb.tail, 0);
}

static void test_single_push_pop(void) {
    ring_buffer_t rb;
    rb_init(&rb);

    CHECK(rb_push(&rb, 0xA5) == true);
    CHECK(rb_is_empty(&rb) == false);
    CHECK(rb_is_full(&rb) == false);

    uint8_t out = 0x00;
    CHECK(rb_pop(&rb, &out) == true);
    CHECK_EQ_U(out, 0xA5);
    CHECK(rb_is_empty(&rb) == true);
}

static void test_pop_on_empty_leaves_out_alone(void) {
    ring_buffer_t rb;
    rb_init(&rb);

    uint8_t out = 0x42;             /* sentinel */
    CHECK(rb_pop(&rb, &out) == false);
    CHECK_EQ_U(out, 0x42);          /* must NOT have been modified */
}

static void test_push_until_full(void) {
    ring_buffer_t rb;
    rb_init(&rb);

    /* Effective capacity is RB_CAPACITY - 1 (one slot reserved). */
    for (uint8_t i = 0; i < RB_CAPACITY - 1U; i++) {
        CHECK(rb_push(&rb, (uint8_t)(0x10 + i)) == true);
    }
    CHECK(rb_is_full(&rb) == true);
    CHECK(rb_is_empty(&rb) == false);

    /* One more push must fail and not corrupt anything. */
    CHECK(rb_push(&rb, 0xFF) == false);
    CHECK(rb_is_full(&rb) == true);
}

static void test_fifo_order(void) {
    ring_buffer_t rb;
    rb_init(&rb);

    for (uint8_t i = 0; i < RB_CAPACITY - 1U; i++) {
        CHECK(rb_push(&rb, (uint8_t)(0x10 + i)) == true);
    }
    for (uint8_t i = 0; i < RB_CAPACITY - 1U; i++) {
        uint8_t out = 0;
        CHECK(rb_pop(&rb, &out) == true);
        CHECK_EQ_U(out, (uint8_t)(0x10 + i));
    }
    CHECK(rb_is_empty(&rb) == true);
}

static void test_wraparound(void) {
    ring_buffer_t rb;
    rb_init(&rb);

    /* Cycle far past capacity to force the head/tail wrap math
     * to be exercised many times. */
    const uint16_t cycles = 100;
    uint8_t expected = 0;
    uint8_t produced = 0;

    for (uint16_t i = 0; i < cycles; i++) {
        /* Push three, pop three. */
        for (int k = 0; k < 3; k++) {
            CHECK(rb_push(&rb, produced++) == true);
        }
        for (int k = 0; k < 3; k++) {
            uint8_t out = 0;
            CHECK(rb_pop(&rb, &out) == true);
            CHECK_EQ_U(out, expected++);
        }
    }
    CHECK(rb_is_empty(&rb) == true);
}

/* This test enforces the SPSC producer/consumer split:
 *   - rb_push must NEVER write tail
 *   - rb_pop  must NEVER write head
 * If either function touches the other side's index, the lock-free
 * property breaks the moment a real ISR fires mid-call.
 *
 * We verify by snapshotting the "other side's" index before the call
 * and checking it is unchanged afterward, across a variety of
 * buffer states. */
static void test_producer_consumer_split(void) {
    ring_buffer_t rb;
    rb_init(&rb);

    /* push on empty: tail must not move */
    {
        uint8_t tail_before = rb.tail;
        CHECK(rb_push(&rb, 0x01) == true);
        CHECK_EQ_U(rb.tail, tail_before);
    }

    /* pop on non-empty: head must not move */
    {
        uint8_t head_before = rb.head;
        uint8_t out = 0;
        CHECK(rb_pop(&rb, &out) == true);
        CHECK_EQ_U(rb.head, head_before);
    }

    /* push on full: tail must not move (and call must fail) */
    rb_init(&rb);
    for (uint8_t i = 0; i < RB_CAPACITY - 1U; i++) {
        (void)rb_push(&rb, i);
    }
    {
        uint8_t tail_before = rb.tail;
        CHECK(rb_push(&rb, 0xEE) == false);
        CHECK_EQ_U(rb.tail, tail_before);
    }

    /* pop on empty: head must not move (and call must fail) */
    rb_init(&rb);
    {
        uint8_t head_before = rb.head;
        uint8_t out = 0;
        CHECK(rb_pop(&rb, &out) == false);
        CHECK_EQ_U(rb.head, head_before);
    }

    /* push mid-buffer (after some pops, indices non-zero):
     * tail must not move. */
    rb_init(&rb);
    for (uint8_t i = 0; i < 5; i++) (void)rb_push(&rb, i);
    for (uint8_t i = 0; i < 3; i++) { uint8_t o; (void)rb_pop(&rb, &o); }
    {
        uint8_t tail_before = rb.tail;
        CHECK(rb_push(&rb, 0xC3) == true);
        CHECK_EQ_U(rb.tail, tail_before);
    }
    /* and pop in that state: head must not move. */
    {
        uint8_t head_before = rb.head;
        uint8_t out = 0;
        CHECK(rb_pop(&rb, &out) == true);
        CHECK_EQ_U(rb.head, head_before);
    }
}

int main(void) {
    test_init_and_empty();
    test_single_push_pop();
    test_pop_on_empty_leaves_out_alone();
    test_push_until_full();
    test_fifo_order();
    test_wraparound();
    test_producer_consumer_split();

    int passed = tests_run - tests_failed;
    printf("\n%d/%d tests passed\n", passed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
