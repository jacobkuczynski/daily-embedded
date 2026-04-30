# Hints — read one at a time

## Hint 1 — power-of-two means a single AND for wrap

Because `RB_CAPACITY` is a power of two, the wrap is `(idx + 1) & RB_MASK`, not `(idx + 1) % RB_CAPACITY`. AND is one cycle on every MCU; modulo by a non-constant compiles to a divide on a Cortex-M0 and a libcall on smaller chips.

The four predicates you need:

- **empty:**  `head == tail`
- **full:**   `((head + 1) & RB_MASK) == tail`
- **count:**  `(head - tail) & RB_MASK`  (not asked for here, but worth knowing)

The `head == tail` ambiguity is exactly why one slot stays reserved.

## Hint 2 — write the data BEFORE advancing the index

In `rb_push`:

```c
rb->buf[rb->head] = byte;                          /* (1) data first  */
rb->head = (uint8_t)((rb->head + 1U) & RB_MASK);   /* (2) then index  */
```

Why the order matters: the consumer's "is there data?" check reads `head`. If you advance `head` first and the consumer interrupts you (or, on a real MCU, runs concurrently on another core or after a memory-store reorder), it will see a `head` pointing at a slot whose contents have not been written yet — it will pop garbage.

The same rule mirrored applies in `rb_pop`: read `buf[tail]` *before* you advance `tail`. Otherwise the producer can see a `tail` advance and overwrite the slot before you've finished reading it.

This ordering is the *whole reason* SPSC is safe without a lock. On a Cortex-M0/M3 with an in-order pipeline and `volatile` indices, this source order is also the run-time order. On a Cortex-M7 or anything with reordering, you'd add a `__DMB()` between the two stores. Mention that in an interview if they push.

## Hint 3 — keep the producer/consumer split honest

The biggest junior mistake on this question is "I'll just track a `count` field and update it in both push and pop." Don't. Now both contexts write the same variable, you've reintroduced a race, and you need a critical section after all.

The discipline:

- `rb_push` reads `head` and `tail`, writes `head`, writes `buf[]`. Period.
- `rb_pop`  reads `head` and `tail`, writes `tail`, reads `buf[]`. Period.

The producer-consumer-split test in `tests.c` enforces exactly this — it snapshots the "other side's" index before each call and asserts it didn't move. If your implementation tracks a shared count, that test will fail.

If you genuinely need a `count()` function, derive it on read: `(head - tail) & RB_MASK`. No new state.
