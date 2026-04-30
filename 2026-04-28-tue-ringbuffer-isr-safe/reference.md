# Reference solution — Tuesday 2026-04-28

Canonical solution. Diff this against your own attempt — the deltas in idiom (`volatile` placement, the order of write-then-advance, AND-mask wrap, exact-width types) are the parts an embedded interviewer would call out around ISR safety.

## `src/ringbuffer.c`

```c
#include "ringbuffer.h"

void rb_init(ring_buffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
    /* buf[] intentionally not cleared — uninitialized slots are
     * never read because they sit outside [tail, head). */
}

bool rb_is_empty(const ring_buffer_t *rb) {
    return rb->head == rb->tail;
}

bool rb_is_full(const ring_buffer_t *rb) {
    /* One slot is reserved so head==tail unambiguously means empty.
     * Therefore "full" is one slot before tail (mod RB_CAPACITY). */
    return (uint8_t)((rb->head + 1U) & RB_MASK) == rb->tail;
}

bool rb_push(ring_buffer_t *rb, uint8_t byte) {
    /* Producer side. Snapshot the consumer's tail once; everything
     * else we touch is the producer's. */
    const uint8_t head = rb->head;
    const uint8_t next = (uint8_t)((head + 1U) & RB_MASK);

    if (next == rb->tail) {
        return false;          /* full — must NOT touch tail or buf */
    }

    /* Order matters: write the byte BEFORE advancing head, so a
     * consumer that observes the new head also sees the new data. */
    rb->buf[head] = byte;
    rb->head = next;
    return true;
}

bool rb_pop(ring_buffer_t *rb, uint8_t *out) {
    /* Consumer side. */
    const uint8_t tail = rb->tail;

    if (rb->head == tail) {
        return false;          /* empty — leave *out alone */
    }

    /* Order matters: read the byte BEFORE advancing tail, so a
     * producer that observes the new tail does not overwrite a
     * slot we are still reading. */
    *out = rb->buf[tail];
    rb->tail = (uint8_t)((tail + 1U) & RB_MASK);
    return true;
}
```

## What an interviewer is looking for

- **`head` and `tail` are `volatile`.** Without `volatile`, the compiler is allowed to hoist `rb->head` out of a loop in the consumer or cache `rb->tail` across the body of `rb_push` in the producer — at which point the queue is broken. Mention this even if the test harness doesn't catch it; the interviewer will.
- **Producer-only writes `head`; consumer-only writes `tail`.** This is the property that lets SPSC be lock-free. The `rb_push` body never assigns to `tail`; the `rb_pop` body never assigns to `head`. The `test_producer_consumer_split` test enforces this exactly.
- **Power-of-two capacity → AND for wrap.** `(idx + 1) & RB_MASK` is one cycle; modulo by a runtime value is a divide on Cortex-M0/M3 (slow) or a libcall on smaller MCUs (very slow). Calling out the cost matters in an embedded interview.
- **Reserved slot disambiguates full vs. empty.** `head == tail` means empty; full is `(head+1) & MASK == tail`. The trade-off (you lose one slot of capacity) is the standard answer; the alternative is a separate `count` field, which reintroduces a shared write and is the wrong call here.
- **Data-store before index-store in `rb_push`; data-load before index-store in `rb_pop`.** This is what makes the queue safe for a real concurrent ISR. On a Cortex-M0/M3 with `volatile`, source order is execution order. On a Cortex-M7 (with reordering), you'd insert a `__DMB()` between the two. Saying "and on M7 I'd add a barrier" is what separates the senior answer from the junior answer.
- **Snapshot the other side's index once.** In the reference, `rb_push` reads `rb->tail` exactly once and `rb_pop` reads `rb->head` exactly once. Re-reading a `volatile` variable in the same function is a common reader-confusing tic and can also mask races in code review.
- **Exact-width types and casts.** `(uint8_t)((head + 1U) & RB_MASK)` — the `1U` keeps the shift unsigned, and the `(uint8_t)` cast silences `-Wconversion` cleanly.

## Variations you might be asked

- **"Now make it fixed-size T, not just bytes."** Parameterize `T buf[RB_CAPACITY]` and pass `sizeof(T)` around, or generate it with a macro. The control flow is identical.
- **"What if the ISR can fire on a multi-core SoC?"** Now `volatile` isn't enough — you need acquire/release semantics. On C11, `_Atomic uint8_t` head/tail with `atomic_store_explicit(memory_order_release)` on the producer side and `atomic_load_explicit(memory_order_acquire)` on the consumer side. On bare-metal C99, a `__DMB()` between the data store and the index store.
- **"What's the failure mode if I forget `volatile`?"** The producer can spin on a stale `tail` and never see the consumer drain — buffer wedges as full forever. Or the consumer caches `head` in a register inside its drain loop and never sees new bytes — buffer looks empty forever. Both are real bugs that have shipped.
- **"What if I have multiple producers?"** SPSC breaks. You need a `__atomic_fetch_add` (or `LDREX/STREX` on ARM) on the head index, or a critical section, or a different queue (MPMC).

## Common mistakes worth flagging

1. **Forgetting `volatile`.** Code happens to work in optimized release until a future compiler version makes a different hoisting decision. Always `volatile` for variables touched by an ISR and a thread.
2. **Tracking a shared `count` field.** Junior reflex. Now both contexts write the same variable, you've reintroduced a race, and you need a critical section. Derive count on read instead: `(head - tail) & RB_MASK`.
3. **Advancing the index before writing the data** in `rb_push` (or before reading it in `rb_pop`). This is the bug the ordering rules in the header are spelled out to prevent.
4. **Modulo instead of mask.** `head = (head + 1) % RB_CAPACITY`. Functionally correct, costs you a divide per push on chips that don't have one. On a Cortex-M0 that's ~30 cycles inside an ISR.
5. **Using full RB_CAPACITY as the usable size.** If you don't reserve the slot, you can't tell empty from full without an extra field, and you've talked yourself into the shared-count bug above.
