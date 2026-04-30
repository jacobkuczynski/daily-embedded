# Hints — read one at a time

## Hint 1 — the free list lives *inside* the free blocks

You don't need a separate array of free indices or a bitmap. The classic embedded pattern is to thread a singly-linked list through the unused blocks themselves:

```
free_head ──> [block 3]──> [block 1]──> [block 0]──> NULL
              (the rest are currently allocated)
```

When a block is on the free list, its first `sizeof(void *)` bytes hold a pointer to the next free block. When the block gets handed to the user via `pool_alloc`, those bytes become the user's — the embedded pointer is just gone, because we already advanced the head off it.

That's why the API requires `block_size >= sizeof(void *)`: any smaller and we have nowhere to store the chain link.

A typedef that makes the intent obvious:

```c
typedef struct free_node {
    struct free_node *next;
} free_node_t;
```

The pool struct holds `void *free_head`. Cast it to `free_node_t *` when you need to walk the chain.

## Hint 2 — build the chain once, at init

`pool_init` is the only place that needs to walk every block. Compute `n = buffer_size / block_size`, then for each block index `i`, write its `next` pointer to point at block `i + 1` (and the last block's next to `NULL`). Set `free_head` to block 0.

Use a wider type when computing the byte offset:

```c
uint8_t *block_i = (uint8_t *)buffer + (uint32_t)i * block_size;
```

so the multiplication doesn't overflow on a 16-bit MCU.

After `pool_init`, the runtime path is tiny:

```c
void *pool_alloc(pool_t *p) {
    if (p->free_head == NULL) return NULL;
    free_node_t *node = (free_node_t *)p->free_head;
    p->free_head = node->next;
    p->free_count--;
    return node;
}
```

## Hint 3 — `pool_free` needs a sanity check, not just trust

The contract says freeing a foreign or misaligned pointer must be a no-op, not a crash. Two checks are enough:

1. The address must lie in `[buffer_start, buffer_end)`.
2. The offset from `buffer_start` must be a multiple of `block_size` — otherwise it's not a block boundary, just an interior pointer the user computed by hand.

```c
uintptr_t addr = (uintptr_t)block;
uintptr_t lo   = (uintptr_t)p->buffer_start;
uintptr_t hi   = (uintptr_t)p->buffer_end;
if (addr < lo || addr >= hi)              return;
if (((addr - lo) % p->block_size) != 0U)  return;
```

Without these, a wild pointer gets pushed onto the free list and the next `pool_alloc` hands a corrupted block back to the user. That's the kind of bug that survives in firmware for years before someone connects the dots between an unrelated free and a downstream crash.
