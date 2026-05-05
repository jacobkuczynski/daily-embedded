# Hints — read one at a time

## Hint 1 — every read-modify-write goes inside a critical section

The skeleton of every public RMW op is the same:

```c
bool sem_post(sem_t *s) {
    bool ok;
    cs_enter();
    if (s->count < s->max_count) {
        s->count = (uint16_t)(s->count + 1U);
        ok = true;
    } else {
        ok = false;
    }
    cs_exit();
    return ok;
}
```

The reason this matters: between "read `count`" and "write `count + 1`", an ISR can fire and post on the same semaphore. Without the CS, two concurrent `sem_post`s on a count of 4 (cap 10) can both read 4, both write 5, and you've lost an event. The classic lost-update race.

Note `s->count + 1U` is computed in `unsigned int` (16 bits is the C-standard minimum) and cast back to `uint16_t`. This silences `-Wconversion` and is also correct on a 16-bit MCU where `int` is 16 bits — `count + 1` would overflow at 65535 without the explicit unsigned, though the cap check rules that out for this problem.

## Hint 2 — single exit point, or be very careful with early returns

The dangerous pattern:

```c
bool sem_try_take_n(sem_t *s, uint16_t n) {
    cs_enter();
    if (s->count < n) {
        return false;          /* BUG: leaves CS held forever */
    }
    s->count -= n;
    cs_exit();
    return true;
}
```

Pick one of:

1. Single exit point — assign to a local `bool ok`, do `cs_exit()` once at the bottom, return `ok`.
2. Mirrored exits — `cs_exit()` immediately before *every* `return`.

The tests catch this either way: `cs_depth` must be 0 when each public function returns. A leaked CS shows up on the very next test as "cs_enter_count != cs_exit_count" or as a depth that grows without bound.

In a real RTOS this bug also causes the kernel to deadlock the first time a higher-priority task tries to run, because interrupts never get re-enabled.

## Hint 3 — `sem_try_take_n` is all-or-nothing, and the zero-take is a no-op success

Two subtleties on the multi-take:

1. **All-or-nothing.** If `count < n`, do not decrement at all. A partial decrement (take what you can, return false) is worse than useless — it's the kind of API that turns a queue-overflow into corrupted accounting that you'll chase for two days. The check happens inside the CS so no one can make the count go up between your `count < n` test and the decrement.

2. **`n == 0` is success with no state change.** `count >= 0` is trivially true for an unsigned, so the natural code returns `true` and decrements by 0 — which is correct: "you asked to take zero items, you got zero items, the world is unchanged." The test exercises this.

While you're there: notice that `sem_init` does *not* take a critical section. That's deliberate — initialization runs during boot before the object is exposed to any ISR, so there's no one to race with. Adding a CS there isn't wrong but it's noise. The interview signal is knowing *when* you don't need the CS, not just slapping one on every function.
