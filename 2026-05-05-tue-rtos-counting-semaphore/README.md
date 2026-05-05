# Tuesday — Counting Semaphore with Critical-Section Discipline

**Topic:** Concurrency, RTOS primitives, ISR-safe critical sections
**Difficulty:** Easy
**Time budget:** 20 minutes
**Date:** 2026-05-05

## Scenario

You're building a tiny piece of an RTOS kernel for a 16-bit MCU: a **counting semaphore**. Producers (any context — thread or ISR) call `sem_post` to release a unit of resource; consumers (thread context) call `sem_try_take` to claim one without blocking. The semaphore's `count` is shared mutable state read and written from both contexts, so every read-modify-write must run inside a critical section.

The platform exposes a CS API as two `extern` functions:

```c
void cs_enter(void);   /* disable interrupts (with appropriate primask save in real life) */
void cs_exit(void);    /* re-enable interrupts */
```

For unit testing on the host, `tests.c` defines stub implementations that count `cs_enter`/`cs_exit` calls and track the current nesting depth. The tests assert that every public function call leaves the CS depth at 0 when it returns — i.e. that you never `return` from inside a critical section without exiting first.

## API

You implement the five functions declared in `src/sem.h`:

1. `sem_init(sem, initial, max_count)` — set up the semaphore. Clamp `initial` to `max_count`. Init runs before the object is shared with any ISR, so no CS is required here.
2. `sem_post(sem)` — increment `count` if `count < max_count`. Return `true` on success, `false` if already at the cap. Must be safe to call from an ISR.
3. `sem_try_take(sem)` — decrement `count` if `count > 0`. Return `true` on success, `false` if empty. Non-blocking.
4. `sem_try_take_n(sem, n)` — atomic all-or-nothing decrement of `n` units. If `count >= n`, decrement by `n` and return `true`. If `count < n`, leave the count untouched and return `false`. (No partial decrement — that would let a consumer who can't take everything still drain the pool.)
5. `sem_count(sem)` — return a snapshot of `count`. On a 16-bit MCU a 16-bit aligned read is one cycle, but a `volatile` 16-bit field can still tear under interrupt nesting depending on the compiler — wrap the read in a CS for portability.

## Constraints

- Pure C99.
- No `malloc`, no stdlib beyond `<stdint.h>`, `<stdbool.h>`, `<stdio.h>`, `<assert.h>`.
- Code must be correct on a 16-bit MCU — don't assume `int` is 32 bits.
- Use exact-width types (`uint16_t`).
- The `count` field is `volatile` because an ISR can modify it.
- Compiles cleanly with `-Wall -Wextra -Wpedantic`.

## Build and test

From this folder:

```
make test
```

Tests in `tests.c` will be red until you implement the functions. They cover:

- init clamps `initial` to `max_count`
- `sem_post` saturates at `max_count` and returns `false` when full
- `sem_try_take` drains to 0 and returns `false` when empty
- `sem_try_take_n` is all-or-nothing (no partial decrement on under-supply)
- `sem_count` returns the current snapshot
- CS balance: `cs_enter` and `cs_exit` calls match across each operation, and depth is 0 on every return path
- Every RMW operation actually enters a critical section at least once (catches "I forgot to lock the post path because it's just one line")

## Hints

`hints.md` has three progressive hints. Read them one at a time only if you're stuck.

## Reference

`reference.md` lands in this folder the morning after the problem ships. Diff your attempt against it.
