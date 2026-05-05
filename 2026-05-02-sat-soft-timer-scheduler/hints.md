# Hints — read one at a time

## Hint 1 — the data structure is just a flat array with an `active` bit

There is no priority queue, no linked list, no heap. The whole scheduler is one fixed-size array of `soft_timer_t`. The `active` bit *is* the free list:

- **schedule:** linear-scan for the first slot with `active == false`, fill it in, set `active = true`. O(n).
- **cancel:** linear-scan for the first match on `(cb, ctx)` with `active == true`, set `active = false`. O(n).
- **dispatch:** linear-scan, fire any active slot whose `deadline_ms <= now`. O(n).

`SCHED_MAX_TIMERS` is 8, so O(n) is fine. A real RTOS would use a min-heap keyed by deadline, but at n=8 the constant-factor wins of a flat array dominate, and the code stays auditable in two dozen lines.

## Hint 2 — `now_ms` needs critical sections, but the tick ISR doesn't

`now_ms` is `volatile uint32_t`. On a 32-bit MCU the compiler will lower a read to a single load and you'd be fine, but the problem statement says the target is 16-bit — the compiler emits two 16-bit loads, and the ISR can fire between them. So:

- `scheduler_tick_isr` is the *only* writer of `now_ms`. It does `s->now_ms = s->now_ms + 1U;`. No critical section — it's already in an ISR, and there are no other writers to race with.
- `scheduler_now` and the snapshot at the top of `scheduler_dispatch` are 32-bit reads from a context the ISR can preempt. Wrap each read in `critical_enter()` / `critical_exit()`, copy into a local, return / use the local.
- `scheduler_schedule` also computes `now + delay_ms`, so it needs the same pattern when reading `now_ms`.
- `scheduler_cancel` doesn't touch `now_ms` at all and doesn't need a critical section.

The `timers[]` array itself is single-context (only the main loop reads or writes it), so it doesn't need protecting either.

## Hint 3 — free the slot BEFORE invoking the callback

The dispatch loop has to do exactly four things, in this order, for each slot it fires:

```c
if (s->timers[i].active && s->timers[i].deadline_ms <= now_snapshot) {
    timer_cb_t cb = s->timers[i].cb;     /* 1. capture into locals       */
    void      *ctx = s->timers[i].ctx;
    s->timers[i].active = false;          /* 2. free the slot             */
    cb(ctx);                              /* 3. invoke (may re-schedule) */
    fired++;                              /* 4. count                     */
}
```

If you do `cb(ctx)` first and `active = false` second, a callback that calls `scheduler_schedule(...)` to re-arm itself sees the table as full (the slot is still marked active) and the re-schedule fails. This is the single most common bug in soft-timer implementations.

The other subtlety is the *snapshot*: read `now_ms` exactly once at the top of `scheduler_dispatch` and use that local for the whole pass. If you re-read it inside the loop, a tick that fires mid-pass can promote a `delay_ms == 0` timer (just scheduled by a callback) to "due", and you'll fire it on the same pass — unbounded work and surprising user-visible behaviour. The test harness checks for this.
