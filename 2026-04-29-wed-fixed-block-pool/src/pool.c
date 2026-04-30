#include "pool.h"

void pool_init(pool_t *p, void *buffer, uint16_t buffer_size, uint16_t block_size) {
    /* TODO: zero the bookkeeping. If block_size < sizeof(void*) or
     *       buffer_size < block_size, leave capacity = 0 and the
     *       free list empty. Otherwise carve `buffer` into
     *       floor(buffer_size / block_size) blocks and thread them
     *       onto a singly-linked free list whose `next` pointer is
     *       stored in the first sizeof(void*) bytes of each free block.
     *       Set free_head to block 0; set capacity and free_count to n. */
    (void)p; (void)buffer; (void)buffer_size; (void)block_size;
}

void *pool_alloc(pool_t *p) {
    /* TODO: if free_head is NULL, return NULL. Otherwise pop free_head,
     *       advance it to the embedded next-pointer, decrement
     *       free_count, and return the popped block. */
    (void)p;
    return NULL;
}

void pool_free(pool_t *p, void *block) {
    /* TODO: ignore NULL. Range-check: address must lie in
     *       [buffer_start, buffer_end) and on a block boundary
     *       (offset % block_size == 0). Then push onto free_head
     *       (write the previous free_head into the block's first
     *       sizeof(void*) bytes) and increment free_count. */
    (void)p; (void)block;
}

uint16_t pool_count_free(const pool_t *p) {
    /* TODO: return the current free count. */
    (void)p;
    return 0;
}

uint16_t pool_capacity(const pool_t *p) {
    /* TODO: return the constant capacity set at init. */
    (void)p;
    return 0;
}
