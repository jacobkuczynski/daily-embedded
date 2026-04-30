# Tuesday — ISR-Safe SPSC Ring Buffer (UART RX)

**Topic:** Concurrency / ISR safety, single-producer / single-consumer queue
**Difficulty:** Easy
**Time budget:** 20 minutes
**Date:** 2026-04-28

## Scenario

You're writing a UART driver for a 16-bit microcontroller. Each time a byte arrives, the hardware fires an RX interrupt; the ISR must drop that byte into a queue and return *fast*. The main loop pulls bytes out of the same queue and parses them into messages.

This is the textbook **single-producer / single-consumer (SPSC) ring buffer**: the ISR is the only producer, the main loop is the only consumer, and you want the queue to be safe to access from both contexts **without disabling interrupts** on every push or pop.

You're implementing the queue's five public functions. The trick is not the indexing math — it's getting the `volatile` discipline and the producer/consumer split right so a real ISR firing mid-`pop()` can't corrupt anything.

## Buffer layout

```c
#define RB_CAPACITY 16U          /* must be a power of two */
#define RB_MASK     (RB_CAPACITY - 1U)

typedef struct {
    volatile uint8_t head;       /* next slot to write — PRODUCER writes, consumer only reads */
    volatile uint8_t tail;       /* next slot to read  — CONSUMER writes, producer only reads */
    uint8_t buf[RB_CAPACITY];
} ring_buffer_t;
```

One slot is always reserved so `head == tail` unambiguously means **empty** and `(head+1) & MASK == tail` means **full**. Effective usable capacity is therefore `RB_CAPACITY - 1` (= 15) bytes.

## Tasks

Implement the five functions in `src/ringbuffer.c`. Their contracts are documented in `src/ringbuffer.h`.

1. `rb_init(rb)` — zero `head` and `tail`. (You don't need to clear `buf`.)
2. `rb_is_empty(rb)` — true when no bytes are queued.
3. `rb_is_full(rb)` — true when one more `push` would overrun.
4. `rb_push(rb, byte)` — **producer side** (called from the ISR). Returns `true` on success, `false` if the buffer is full. Must not modify `tail`.
5. `rb_pop(rb, out)` — **consumer side** (called from the main loop). Pops one byte into `*out`; returns `true` on success, `false` if empty. On empty, `*out` is left untouched. Must not modify `head`.

## Constraints

- Pure C99.
- No `malloc`, no stdlib beyond `<stdint.h>`, `<stdbool.h>`.
- Code must be correct on a 16-bit MCU — don't assume `int` is 32 bits.
- Use exact-width types (`uint8_t`).
- Compiles cleanly with `-Wall -Wextra -Wpedantic`.
- **Producer/consumer split is a hard requirement:** `rb_push` must never write `tail`; `rb_pop` must never write `head`. This is what makes the queue lock-free for SPSC. Tests check it.
- Inside `rb_push`, write the data byte into `buf[head]` *before* advancing `head`. Inside `rb_pop`, read `buf[tail]` *before* advancing `tail`. The order matters even on a Mac harness because that's the property that makes a real ISR firing mid-call safe.

## Build and test

From this folder:

```
make test
```

Tests in `tests.c` exercise the obvious cases plus a few that catch common SPSC mistakes: full/empty disambiguation, FIFO order across a wrap-around, `rb_pop` on empty leaving `*out` alone, and the producer/consumer-split invariant.

## Hints

`hints.md` has three progressive hints. Read them one at a time only if you're stuck.

## Reference

`reference.md` lands in this folder the morning after the problem ships. Diff your attempt against it.
