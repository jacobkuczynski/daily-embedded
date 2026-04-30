#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include "src/pool.h"

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

/* 16-byte blocks * 4 = 64 bytes. uintptr_t storage gives us at least
 * void*-alignment on every supported platform — no _Alignas needed. */
#define BLOCK_SIZE  16U
#define NUM_BLOCKS  4U
#define BUF_BYTES   (BLOCK_SIZE * NUM_BLOCKS)

static uintptr_t aligned_buf[BUF_BYTES / sizeof(uintptr_t)];

static void *buf(void) { return (void *)aligned_buf; }

/* Helper: drain the pool, returning how many allocs succeeded. */
static uint16_t drain(pool_t *p) {
    uint16_t n = 0;
    while (pool_alloc(p) != NULL) n++;
    return n;
}

static void test_init_capacity(void) {
    pool_t p;
    pool_init(&p, buf(), BUF_BYTES, BLOCK_SIZE);
    CHECK_EQ_U(pool_capacity(&p), NUM_BLOCKS);
    CHECK_EQ_U(pool_count_free(&p), NUM_BLOCKS);
}

static void test_init_buffer_too_small(void) {
    pool_t p;
    /* buffer holds zero whole blocks */
    pool_init(&p, buf(), (uint16_t)(BLOCK_SIZE - 1U), BLOCK_SIZE);
    CHECK_EQ_U(pool_capacity(&p), 0);
    CHECK_EQ_U(pool_count_free(&p), 0);
    CHECK(pool_alloc(&p) == NULL);
}

static void test_init_block_smaller_than_pointer(void) {
    pool_t p;
    /* block_size below sizeof(void*) — can't store the free-list pointer */
    pool_init(&p, buf(), BUF_BYTES, 1U);
    CHECK_EQ_U(pool_capacity(&p), 0);
    CHECK(pool_alloc(&p) == NULL);
}

static void test_alloc_exhaust(void) {
    pool_t p;
    pool_init(&p, buf(), BUF_BYTES, BLOCK_SIZE);

    void *blocks[NUM_BLOCKS];
    for (uint16_t i = 0; i < NUM_BLOCKS; i++) {
        blocks[i] = pool_alloc(&p);
        CHECK(blocks[i] != NULL);
    }
    /* exhausted */
    CHECK(pool_alloc(&p) == NULL);
    CHECK_EQ_U(pool_count_free(&p), 0);

    /* All returned blocks must be distinct and inside the buffer. */
    uintptr_t lo = (uintptr_t)buf();
    uintptr_t hi = lo + BUF_BYTES;
    for (uint16_t i = 0; i < NUM_BLOCKS; i++) {
        uintptr_t a = (uintptr_t)blocks[i];
        CHECK(a >= lo && a < hi);
        for (uint16_t j = (uint16_t)(i + 1U); j < NUM_BLOCKS; j++) {
            CHECK(blocks[i] != blocks[j]);
        }
    }
}

static void test_free_returns_block(void) {
    pool_t p;
    pool_init(&p, buf(), BUF_BYTES, BLOCK_SIZE);

    void *a = pool_alloc(&p);
    void *b = pool_alloc(&p);
    CHECK(a != NULL && b != NULL);
    CHECK_EQ_U(pool_count_free(&p), NUM_BLOCKS - 2U);

    pool_free(&p, a);
    CHECK_EQ_U(pool_count_free(&p), NUM_BLOCKS - 1U);
    pool_free(&p, b);
    CHECK_EQ_U(pool_count_free(&p), NUM_BLOCKS);

    /* Round-trip: should be able to re-alloc capacity blocks again. */
    CHECK_EQ_U(drain(&p), NUM_BLOCKS);
    CHECK(pool_alloc(&p) == NULL);
}

static void test_free_null_is_noop(void) {
    pool_t p;
    pool_init(&p, buf(), BUF_BYTES, BLOCK_SIZE);
    pool_free(&p, NULL);
    CHECK_EQ_U(pool_count_free(&p), NUM_BLOCKS);
    CHECK_EQ_U(drain(&p), NUM_BLOCKS);
}

static void test_free_out_of_range_is_noop(void) {
    pool_t p;
    pool_init(&p, buf(), BUF_BYTES, BLOCK_SIZE);

    /* Storage that did not come from this pool. */
    uintptr_t bogus_storage[2] = {0};

    pool_free(&p, &bogus_storage[0]);
    CHECK_EQ_U(pool_count_free(&p), NUM_BLOCKS);

    /* Drain the pool, then try to "free" a foreign pointer.
     * Free count must stay zero, and alloc must still return NULL —
     * proving the bogus pointer was not pushed onto the free list. */
    CHECK_EQ_U(drain(&p), NUM_BLOCKS);
    CHECK_EQ_U(pool_count_free(&p), 0);
    pool_free(&p, &bogus_storage[0]);
    CHECK_EQ_U(pool_count_free(&p), 0);
    CHECK(pool_alloc(&p) == NULL);
}

static void test_free_misaligned_pointer_is_noop(void) {
    pool_t p;
    pool_init(&p, buf(), BUF_BYTES, BLOCK_SIZE);

    /* A pointer inside the buffer but NOT on a block boundary —
     * e.g., 4 bytes into block 0. Must be ignored. */
    void *interior = (void *)((uint8_t *)buf() + 4U);
    pool_free(&p, interior);
    CHECK_EQ_U(pool_count_free(&p), NUM_BLOCKS);
    CHECK_EQ_U(drain(&p), NUM_BLOCKS);
}

static void test_blocks_dont_overlap(void) {
    pool_t p;
    pool_init(&p, buf(), BUF_BYTES, BLOCK_SIZE);

    /* Alloc all blocks, write a unique signature into each, then verify
     * each block still holds its own signature. Catches overlapping
     * blocks, wrong block stride, and free-list pointer leakage into
     * user data. */
    uint8_t *blocks[NUM_BLOCKS];
    for (uint16_t i = 0; i < NUM_BLOCKS; i++) {
        blocks[i] = (uint8_t *)pool_alloc(&p);
        CHECK(blocks[i] != NULL);
        if (blocks[i] != NULL) {
            for (uint16_t j = 0; j < BLOCK_SIZE; j++) {
                blocks[i][j] = (uint8_t)(i + 1U);
            }
        }
    }
    for (uint16_t i = 0; i < NUM_BLOCKS; i++) {
        if (blocks[i] != NULL) {
            for (uint16_t j = 0; j < BLOCK_SIZE; j++) {
                CHECK_EQ_U(blocks[i][j], (uint8_t)(i + 1U));
            }
        }
    }
}

int main(void) {
    test_init_capacity();
    test_init_buffer_too_small();
    test_init_block_smaller_than_pointer();
    test_alloc_exhaust();
    test_free_returns_block();
    test_free_null_is_noop();
    test_free_out_of_range_is_noop();
    test_free_misaligned_pointer_is_noop();
    test_blocks_dont_overlap();

    int passed = tests_run - tests_failed;
    printf("\n%d/%d tests passed\n", passed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
