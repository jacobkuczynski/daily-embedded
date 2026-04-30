# Wednesday — DS&A: Fixed-Size Block Pool Allocator

**Topic:** Data structures under embedded constraints — no-malloc allocators
**Difficulty:** Easy
**Time budget:** 20 minutes
**Date:** 2026-04-29

## Scenario

Your firmware can't call `malloc`. It needs to hand out small fixed-size buffers — sensor samples, message frames, scratch records — at runtime, and recycle them when the consumer is done. The textbook answer is a **fixed-size block pool**: carve a static buffer into N equal-sized blocks once at boot, and serve `alloc`/`free` from a free list embedded inside the unused blocks themselves.

You're implementing that pool. Five small functions, all O(1).

## API

```c
typedef struct {
    void     *buffer_start;
    void     *buffer_end;
    void     *free_head;
    uint16_t  block_size;
    uint16_t  capacity;
    uint16_t  free_count;
} pool_t;

void     pool_init(pool_t *p, void *buffer, uint16_t buffer_size, uint16_t block_size);
void    *pool_alloc(pool_t *p);
void     pool_free(pool_t *p, void *block);
uint16_t pool_count_free(const pool_t *p);
uint16_t pool_capacity(const pool_t *p);
```

Full contracts live in `src/pool.h`.

## Tasks

Implement the five functions in `src/pool.c`.

1. `pool_init` — carve `buffer` into `floor(buffer_size / block_size)` blocks and thread them onto a singly-linked free list. If `block_size < sizeof(void *)` or `buffer_size < block_size`, leave the pool empty (`capacity == 0`).
2. `pool_alloc` — pop the free list head and return it. Return `NULL` when exhausted.
3. `pool_free` — push the block back onto the free list. `NULL` and pointers outside `[buffer_start, buffer_end)` must be silently ignored — not crash, not corrupt the free list.
4. `pool_count_free` — current number of available blocks.
5. `pool_capacity` — total blocks the pool was carved into at init (constant after init).

## Constraints

- Pure C99.
- No `malloc`, no stdlib functions beyond `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`.
- Code must be correct on a 16-bit MCU — don't assume `int` is 32 bits. Promote multiplications to `uint32_t` where the byte offset could exceed `UINT16_MAX`.
- Use exact-width types (`uint8_t`, `uint16_t`).
- Caller guarantees that `buffer` is aligned for `void *` and that `block_size` is a multiple of `sizeof(void *)`. Assume that, don't re-validate it.
- Compiles cleanly with `-Wall -Wextra -Wpedantic`.

## Build and test

From this folder:

```
make test
```

Tests in `tests.c` will be red until the functions are implemented. They cover capacity bookkeeping, exhaustion, alloc/free round-trips, the two reject-input edge cases on `pool_init`, and a no-overlap check that writes a signature into each allocated block.

## Hints

`hints.md` has three progressive hints. Read them one at a time only if you're stuck.

## When you're done

Tell Claude "review today's problem". You'll get feedback on correctness, embedded code quality, what an interviewer would call out, and a tighter alternative if yours wasn't the cleanest. Your attempt gets appended to `attempts/`.

Tomorrow morning, `reference.md` lands in this folder with the canonical solution for comparison.
