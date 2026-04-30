#ifndef POOL_H
#define POOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief  Fixed-size block memory pool.
 *
 * Carved out of a caller-supplied buffer at init time. All bookkeeping
 * lives in this struct; nothing is allocated dynamically. Free blocks
 * are threaded onto a singly-linked free list whose `next` pointer is
 * stored inside the unused block memory itself.
 *
 * Constraints (caller-enforced):
 *   - `buffer` is aligned for `void *`.
 *   - `block_size` is a multiple of `sizeof(void *)`.
 *   - `block_size >= sizeof(void *)`.
 */
typedef struct {
    void     *buffer_start;  /* first byte of the buffer */
    void     *buffer_end;    /* one past the last usable byte */
    void     *free_head;     /* head of the singly-linked free list, or NULL */
    uint16_t  block_size;    /* bytes per block */
    uint16_t  capacity;      /* total blocks carved at init */
    uint16_t  free_count;    /* blocks currently on the free list */
} pool_t;

/**
 * @brief  Initialize `p` over `buffer` (`buffer_size` bytes), carving
 *         it into floor(buffer_size / block_size) blocks and threading
 *         them onto the free list.
 *
 *         Edge cases — pool is initialized as empty (capacity == 0):
 *           - block_size < sizeof(void *)
 *           - buffer_size < block_size
 *
 *         The `buffer_start` / `buffer_end` fields are always set,
 *         even when capacity is 0, so that pool_free's range check
 *         behaves predictably on an empty pool.
 */
void pool_init(pool_t *p, void *buffer, uint16_t buffer_size, uint16_t block_size);

/**
 * @brief  Pops one block off the free list and returns it.
 *         Returns NULL if the pool is exhausted.
 *         The contents of the returned block are unspecified.
 */
void *pool_alloc(pool_t *p);

/**
 * @brief  Returns `block` to the pool.
 *
 *         Defensive: NULL or a pointer that did not come from this
 *         pool is silently ignored — does not crash, does not corrupt
 *         the free list. The implementation rejects pointers outside
 *         [buffer_start, buffer_end) and pointers not on a block
 *         boundary.
 */
void pool_free(pool_t *p, void *block);

/**
 * @brief  Number of blocks currently available for `pool_alloc`.
 */
uint16_t pool_count_free(const pool_t *p);

/**
 * @brief  Total number of blocks the pool was carved into at init.
 *         Constant for the lifetime of the pool.
 */
uint16_t pool_capacity(const pool_t *p);

#endif /* POOL_H */
