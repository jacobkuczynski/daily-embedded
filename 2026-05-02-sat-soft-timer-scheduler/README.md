# Saturday — Soft-Timer Scheduler with ISR Tick

**Topic:** Concurrency / ISR safety + DS&A (fixed-size table, no malloc)
**Difficulty:** Medium
**Time budget:** 45–60 minutes
**Date:** 2026-05-02

## Scenario

You're writing the soft-timer subsystem for a 16-bit microcontroller. A 1 kHz hardware timer fires `TICK_IRQHandler`, which calls into your scheduler to advance a millisecond counter. Application code schedules one-shot callbacks ("call `gpio_blink` 250 ms from now"). The main loop periodically calls `scheduler_dispatch()`, which fires every callback whose deadline has passed.

This is the bread-and-butter pattern for any cooperative embedded system without an RTOS. Interviewers like it because the design has only two real questions, and both have to be right at the same time:

1. **DS&A:** How do you store and find the due timers in a fixed-size table without `malloc`, and what happens when a callback re-schedules itself?
2. **Concurrency:** `now_ms` is shared between the tick ISR and the main loop. On a 16-bit MCU a 32-bit read is non-atomic, so reads need a critical section. Schedule/cancel/dispatch are main-loop only — they don't.

## Layout

```c
#define SCHED_MAX_TIMERS 8U

typedef void (*timer_cb_t)(void *ctx);

typedef struct {
    uint32_t   deadline_ms;
    timer_cb_t cb;
    void      *ctx;
    bool       active;
} soft_timer_t;

typedef struct {
    soft_timer_t       timers[SCHED_MAX_TIMERS];
    volatile uint32_t  now_ms;   /* ISR writes; main loop reads under critical section */
} scheduler_t;
```

The `timers[]` array is the entire pool. There is no free list, no heap, no linked structure. `active == false` means the slot is free; `active == true` means it holds a pending timer.

## Mock critical-section primitives

The harness provides:

```c
void critical_enter(void);   /* on a real target: __disable_irq() */
void critical_exit(void);    /* on a real target: __enable_irq()  */
```

Use these to wrap any read of `now_ms` from a context that can be preempted by the tick ISR — which is to say: anywhere except inside `scheduler_tick_isr` itself. The tick ISR is the sole writer of `now_ms`, so it doesn't need them.

The harness counts entries and exits and verifies `depth == 0` after every public API call.

## Tasks

Implement the six functions in `src/scheduler.c`. Their contracts are documented in `src/scheduler.h`.

1. `scheduler_init(s)` — clear `now_ms` to 0 and mark every slot inactive.
2. `scheduler_tick_isr(s)` — increment `now_ms` by 1. Called from the timer ISR.
3. `scheduler_schedule(s, delay_ms, cb, ctx)` — find the first inactive slot, set its deadline to `now + delay_ms`, store `cb` and `ctx`, mark active. Return `true`. If the table is full, return `false`. Reject a `NULL` callback.
4. `scheduler_cancel(s, cb, ctx)` — find the first active slot whose `(cb, ctx)` matches, mark it inactive, return `true`. Return `false` if no such timer exists.
5. `scheduler_dispatch(s)` — fire every active timer whose deadline has been reached. Return the number of callbacks fired this call. Read `now_ms` once at the top, under a critical section, and use that snapshot for the whole pass.
6. `scheduler_now(s)` — return the current tick count, atomically with respect to the tick ISR.

## Constraints

- Pure C99.
- No `malloc`, no stdlib beyond `<stdint.h>`, `<stdbool.h>`, `<stddef.h>` (for `NULL`).
- Code must be correct on a 16-bit MCU — don't assume `int` is 32 bits, and don't assume a 32-bit volatile read is atomic.
- Use exact-width types (`uint8_t`, `uint32_t`).
- Compiles cleanly with `-Wall -Wextra -Wpedantic`.
- **Free the slot before invoking the callback.** A callback that calls `scheduler_schedule` on itself must be able to land in the same slot it just vacated. Tests check this.
- **Snapshot `now_ms` once per dispatch call.** If you read it inside the loop, a tick mid-dispatch can fire a timer you weren't planning to fire this call, and (worse) a callback that schedules a new timer with `delay_ms == 0` could see itself become due and fire on the same pass — unbounded work.

## Build and test

From this folder:

```
make test
```

Tests in `tests.c` cover scheduling at the deadline, no-double-fire after a callback runs, table-full rejection, cancel-before-fire, a callback that re-schedules itself, and a check that `critical_enter/exit` are balanced and actually called from `scheduler_now` and `scheduler_dispatch`.

## Hints

`hints.md` has three progressive hints. Read them one at a time only if you're stuck.

## Reference

`reference.md` lands in this folder the morning after the problem ships. Diff your attempt against it.
