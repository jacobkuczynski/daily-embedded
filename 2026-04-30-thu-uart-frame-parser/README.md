# Thursday — UART Frame Parser State Machine

**Topic:** Peripheral protocols, byte-driven state machines
**Difficulty:** Easy
**Time budget:** 20 minutes
**Date:** 2026-04-30

## Scenario

You're writing the receive-side parser for a small wireline protocol the rest of the firmware speaks over UART. An ISR pulls one byte at a time off the UART RX register and feeds it to your parser; your parser assembles complete frames and signals upward when one is ready.

This is the canonical "byte-by-byte state machine" you'll see in any UART, SPI, or HDLC-style driver. Interviewers like it because it surfaces three classic mistakes quickly: forgetting auto-resync after a bad frame, walking off the end of a fixed buffer when the length byte is malicious, and confusing the SYNC byte with a payload byte that happens to equal SYNC.

## Frame layout

Each frame is a sequence of bytes on the wire:

| Offset | Field    | Width | Notes |
|--------|----------|-------|-------|
| 0      | SYNC     | 1     | Always `0x7E`. Marks start-of-frame. |
| 1      | LEN      | 1     | Length of payload in bytes. Valid range: `1..FRAME_MAX_PAYLOAD`. |
| 2..    | PAYLOAD  | LEN   | Raw payload bytes. May contain `0x7E`. |
| 2+LEN  | CHECKSUM | 1     | XOR of `LEN` and every payload byte. |

`FRAME_MAX_PAYLOAD` is fixed at 32 bytes for this exercise.

## Tasks

Implement the five functions in `src/frame_parser.c`. Their contracts are documented in `src/frame_parser.h`.

1. `frame_parser_reset(p)` — put the parser into the initial state, ready to look for SYNC.
2. `frame_parser_feed(p, byte)` — advance the state machine by one byte. Returns `FRAME_INCOMPLETE`, `FRAME_READY`, or `FRAME_ERROR`.
3. `frame_parser_payload_len(p)` — length of the most recently completed frame's payload. Valid only when the last `feed` returned `FRAME_READY`.
4. `frame_parser_payload(p)` — pointer to the most recently completed frame's payload bytes (length given by `frame_parser_payload_len`). Valid only when the last `feed` returned `FRAME_READY`.
5. `frame_compute_checksum(data, len)` — XOR of `data[0..len-1]`. Pure helper; the TX path uses it to build a frame, and `feed` uses the same idiom internally to verify one.

## Behaviour requirements

- A byte received while in `WAIT_SYNC` that is not `0x7E` is silently discarded. Garbage between frames is normal.
- An invalid `LEN` (zero, or greater than `FRAME_MAX_PAYLOAD`) must return `FRAME_ERROR` and re-arm the parser to look for the next SYNC.
- A checksum mismatch must return `FRAME_ERROR` and re-arm the parser to look for the next SYNC.
- After `FRAME_READY`, the next call to `feed` starts a fresh frame search (so two well-formed frames back-to-back must both be reported).
- A `0x7E` byte that lands in the middle of a payload is just data — the parser is in `WAIT_PAYLOAD` and must store it, not re-sync on it.

## Constraints

- Pure C99.
- No `malloc`, no stdlib functions beyond `<stdint.h>` and `<stdbool.h>`.
- Code must be correct on a 16-bit MCU — don't assume `int` is 32 bits.
- Use exact-width types (`uint8_t`, `uint16_t`) explicitly.
- Compiles cleanly with `-Wall -Wextra -Wpedantic`.

## Build and test

From this folder:

```
make test
```

Tests in `tests.c` will be red until you implement the functions. They cover clean frames, garbage-then-frame, bad checksum, invalid length, back-to-back frames, payload that contains a SYNC byte, and recovery after an error.

## Hints

`hints.md` has three progressive hints. Read them one at a time only if you're stuck.

## When you're done

Tell Claude "review today's problem". You'll get feedback covering correctness, embedded-specific code quality, what an interviewer would call out, and a cleaner alternative if yours wasn't the tightest. Your attempt gets appended to `attempts/`.

Tomorrow morning, `reference.md` lands in this folder with the canonical solution for comparison.
