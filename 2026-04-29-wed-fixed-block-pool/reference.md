# Reference — Fixed-Size Block Pool Allocator (2026-04-29)

## Canonical solution

`src/pool.c`:

```c
#include "pool.h"

/* Each free block stores a pointer to the next free block in its
 * first sizeof(void*) bytes. Going through this typedef keeps the
 * intent obvious in the cast sites below. */
typedef struct free_node {
    struct free_node *next;
} free_node_t;

void pool_init(pool_t *p, void *buffer, uint16_t buffer_size, uint16_t block_size) {
    /* Always set the address window so pool_free's range check is
     * well-defined even when the pool ends up empty. */
    p->buffer_start = buffer;
    p->buffer_end   = (void *)((uint8_t *)buffer + buffer_size);
    p->free_head    = NULL;
    p->block_size   = block_size;
    p->capacity     = 0;
    p->free_count   = 0;

    if (block_size < sizeof(void *)) return;
    if (buffer_size < block_size)    return;

    uint16_t n = (uint16_t)(buffer_size / block_size);

    /* Thread the chain: block 0 -> block 1 -> ... -> block n-1 -> NULL.
     * Widen the offset multiplication to uint32_t so it doesn't overflow
     * on a 16-bit MCU where uint16_t * uint16_t can wrap. */
    uint8_t *base = (uint8_t *)buffer;
    for (uint16_t i = 0; i < n; i++) {
        free_node_t *node = (free_node_t *)(base + (uint32_t)i * block_size);
        if ((uint16_t)(i + 1U) < n) {
            node->next = (free_node_t *)(base + (uint32_t)(i + 1U) * block_size);
        } else {
            node->next = NULL;
        }
    }

    p->free_head  = base;
    p->capacity   = n;
    p->free_count = n;
}

void *pool_alloc(pool_t *p) {
    if (p->free_head == NULL) {
        return NULL;
    }
    free_node_t *node = (free_node_t *)p->free_head;
    p->free_head = node->next;
    p->free_count--;
    return (void *)node;
}

void pool_free(pool_t *p, void *block) {
    if (block == NULL) {
        return;
    }
    /* Range-check: pointer must lie inside [buffer_start, buffer_end)
     * and on a block boundary. Guards against accidental "frees" of
     * pointers we never owned, which would otherwise be silently
     * pushed onto the free list and handed out by the next alloc. */
    uintptr_t addr = (uintptr_t)block;
    uintptr_t lo   = (uintptr_t)p->buffer_start;
    uintptr_t hi   = (uintptr_t)p->buffer_end;
    if (addr < lo || addr >= hi) {
        return;
    }
    if (p->block_size == 0U || ((addr - lo) % p->block_size) != 0U) {
        return;
    }

    free_node_t *node = (free_node_t *)block;
    node->next   = (free_node_t *)p->free_head;
    p->free_head = node;
    p->free_count++;
}

uint16_t pool_count_free(const pool_t *p) {
    return p->free_count;
}

uint16_t pool_capacity(const pool_t *p) {
    return p->capacity;
}
```

## What an interviewer is looking for

- **Free list embedded in the free blocks themselves**, not a side array. This is the idiom — bitmap-based pools and "free indices" arrays both work but are bigger and slower. The interviewer wants to see you reach for the embedded chain first because it's the standard.
- **The minimum-block-size constraint is acknowledged.** `block_size >= sizeof(void *)` isn't arbitrary — it's exactly the space the chain link needs. Calling that out in the header docs and in `pool_init` (returning empty rather than scribbling past the block) is the read-the-spec-carefully signal.
- **`pool_alloc` is three lines and O(1).** Pop, advance, decrement. If the candidate's alloc walks the chain, that's a tell they didn't grasp the layout.
- **`pool_free` validates input.** Real firmware gets called with NULLs, double-frees, and pointers from other pools all the time. Two checks — range and block-boundary — keep the free list intact under abuse. A free-list that takes garbage on faith is the kind of latent bug that bites a year later in some other module.
- **Width-aware arithmetic for a 16-bit target.** `(uint32_t)i * block_size` for the byte offset, `uintptr_t` for the address comparisons. The candidate should not assume `int` is 32 bits.
- **`buffer_end` is set even when capacity is 0.** Otherwise the post-init `pool_free` range check has to special-case empty pools. Initialize the address window unconditionally so every code path downstream works the same.
- **Exact-width types in the struct (`uint16_t`)** instead of `size_t` — this is sized for an embedded target where `size_t` may be 16-bit anyway and where saving 4 bytes of metadata per pool matters.

## Common mistakes worth flagging

1. **Putting the free list in a side array of indices or pointers.** Works, but doubles the metadata footprint and offers nothing in return. The point of a fixed-block pool *is* that the allocator state lives inside the unused blocks.
2. **Forgetting to NULL-terminate the chain at init.** Easy to write the loop as "every block points to the next" and then leave the last block's `next` as whatever garbage was in the buffer. First exhaustion-then-extra-alloc test catches it: alloc returns a wild pointer instead of NULL.
3. **Trusting `pool_free`'s argument.** No range check, no boundary check, just push onto the head. The first time someone passes the address of a stack variable by mistake, the pool returns that stack address from the next alloc, and the symptoms appear three call sites away.
4. **Using `int i` or `size_t` in the offset multiplication and forgetting that `uint16_t * uint16_t` promotes through `int`.** On a 16-bit MCU where `int` is 16-bit, `i * block_size` wraps silently for big buffers. `(uint32_t)i * block_size` is the safe pattern.
5. **Not handling `block_size < sizeof(void *)`.** A naive implementation will happily write a `free_node_t *` into a 1- or 2-byte block and clobber neighboring blocks. Either reject at init (capacity=0) or assert; never ignore.
