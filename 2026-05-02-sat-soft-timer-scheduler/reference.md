# Reference solution — 2026-05-02 — Soft-timer scheduler with ISR tick

## Canonical solution

```c
#include "scheduler.h"

void scheduler_init(scheduler_t *s) {
    s->now_ms = 0U;
    for (uint8_t i = 0; i < SCHED_MAX_TIMERS; i++) {
        s->timers[i].deadline_ms = 0U;
        s->timers[i].cb          = NULL;
        s->timers[i].ctx         = NULL;
        s->timers[i].active      = false;
    }
}

void scheduler_tick_isr(scheduler_t *s) {
    /* Sole writer of now_ms. The 1U keeps the arithmetic unsigned
     * even on a 16-bit MCU where `int` is 16-bit. */
    s->now_ms = s->now_ms + 1U;
}

bool scheduler_schedule(scheduler_t *s, uint32_t delay_ms,
                        timer_cb_t cb, void *ctx) {
    if (cb == NULL) {
        return false;
    }

    /* Read now_ms atomically with respect to the tick ISR. */
    critical_enter();
    uint32_t now = s->now_ms;
    critical_exit();

    for (uint8_t i = 0; i < SCHED_MAX_TIMERS; i++) {
        if (!s->timers[i].active) {
            s->timers[i].deadline_ms = now + delay_ms;
            s->timers[i].cb          = cb;
            s->timers[i].ctx         = ctx;
            s->timers[i].active      = true;
            return true;
        }
    }
    return false;
}

bool scheduler_cancel(scheduler_t *s, timer_cb_t cb, void *ctx) {
    for (uint8_t i = 0; i < SCHED_MAX_TIMERS; i++) {
        if (s->timers[i].active &&
            s->timers[i].cb  == cb &&
            s->timers[i].ctx == ctx) {
            s->timers[i].active = false;
            return true;
        }
    }
    return false;
}

uint8_t scheduler_dispatch(scheduler_t *s) {
    /* Snapshot now_ms once. Using a local for the whole pass means a
     * tick (or a self-rescheduled delay-0 timer) cannot extend the
     * pass into unbounded work. */
    critical_enter();
    uint32_t now_snapshot = s->now_ms;
    critical_exit();

    uint8_t fired = 0;
    for (uint8_t i = 0; i < SCHED_MAX_TIMERS; i++) {
        if (s->timers[i].active && s->timers[i].deadline_ms <= now_snapshot) {
            timer_cb_t cb  = s->timers[i].cb;
            void      *ctx = s->timers[i].ctx;

            /* Free the slot BEFORE invoking the callback so a
             * self-rescheduling callback can land in it. */
            s->timers[i].active = false;

            cb(ctx);
            fired++;
        }
    }
    return fired;
}

uint32_t scheduler_now(const scheduler_t *s) {
    critical_enter();
    uint32_t now = s->now_ms;
    critical_exit();
    return now;
}
```

## What an interviewer is looking for

- **The `active` bit is the free list.** No linked structure, no separate "in-use count", no compaction. A linear scan over a small fixed array is the right answer at `n = 8`. Senior reviewers want to see the simplest data structure that meets the constraints, not a min-heap.
- **`scheduler_tick_isr` does not call `critical_enter/exit`.** It is already in interrupt context — the only thing that could preempt it is a higher-priority interrupt, and we don't have one. Adding a critical section here is the most common over-engineering mistake on this problem.
- **The tick ISR is the *sole* writer of `now_ms`.** That single-writer property is what makes the pattern work: the main-loop reader needs a critical section because the read is non-atomic on a 16-bit MCU, but no critical section is needed to serialise *writers*.
- **Snapshot `now_ms` once at the top of `scheduler_dispatch`.** This bounds the pass to "fire every timer that was due as of entry, no more". Re-reading `now_ms` inside the loop creates a windows where a `delay_ms == 0` timer scheduled by a callback can become due and fire on the same pass — leading to unbounded work and surprising user-visible behaviour.
- **Free the slot before invoking the callback.** The dispatch sequence is: capture `cb` and `ctx` into locals → set `active = false` → call `cb(ctx)`. This is the only ordering that lets a callback safely call `scheduler_schedule` to re-arm itself. The test harness fills 7 of 8 slots and exercises the eighth as a self-rescheduling timer specifically to check this.
- **Exact-width types and unsigned arithmetic.** `uint32_t` deadlines, `uint8_t` slot indices, `1U` in the increment. `delay_ms` is `uint32_t` so `now + delay_ms` is well-defined modular arithmetic — wraparound at ~49 days is a real concern in the field but isn't tested here.
- **`bool` for two-valued state, `enum`/`typedef` discipline.** Function-pointer comparison against `NULL` (via `<stddef.h>`) reads more cleanly than `(timer_cb_t)0` and is what you'd see in production code.
- **`scheduler_cancel` doesn't touch `now_ms`.** No critical section. A common mistake is to pattern-match "this looks shared, throw a critical section at it" and pay 100+ cycles per cancel. The right answer is to know exactly what shares state with the ISR (just `now_ms`) and gate only those reads.

## Common mistakes worth flagging

1. **Calling `cb` with the slot still marked active.** Then a self-rescheduling callback sees the table as full and the re-schedule fails. Test `test_callback_can_reschedule` catches this — it fills 7/8 slots and the 8th must rotate.
2. **Re-reading `now_ms` inside the dispatch loop.** Subtle: works for the single-fire test but breaks `test_zero_delay_inside_callback_defers_to_next_dispatch`, which expects a `delay_ms == 0` timer scheduled by a callback to fire on the *next* dispatch, not this one.
3. **Putting `critical_enter/exit` around the tick ISR.** Wrong on two levels: the ISR is already non-preemptable for our purposes (single writer), and disabling interrupts inside an ISR is at best a no-op and at worst a confusing footgun on cores with interrupt nesting.
4. **Treating `cb == NULL` as a runtime contract violation (assert) instead of returning `false`.** The API contract is "returns false if cb is NULL" — letting it through would store NULL as `active`, and a later dispatch would call NULL and crash. Asserts are fine in development but the public API still has to behave defined-y in release.
5. **Using a signed type for `now_ms` or `delay_ms`.** Either invites signed-overflow UB on the wraparound. The whole tick-arithmetic story works because everything is unsigned modular.
6. **Forgetting to clear `now_ms` in `scheduler_init`.** The struct is on the stack in the test harness, so its fields are uninitialised; without an explicit `s->now_ms = 0`, `scheduler_now(&s)` returns whatever was on the stack, and `test_init`'s `CHECK_EQ_U(scheduler_now(&s), 0)` fails non-deterministically.
