# Reference solution — 2026-04-30 — UART frame parser state machine

## Canonical solution

```c
#include "frame_parser.h"

void frame_parser_reset(frame_parser_t *p) {
    p->state       = PARSE_WAIT_SYNC;
    p->payload_len = 0;
    p->payload_idx = 0;
    p->running_xor = 0;
}

frame_status_t frame_parser_feed(frame_parser_t *p, uint8_t byte) {
    switch (p->state) {

    case PARSE_WAIT_SYNC:
        if (byte == FRAME_SYNC) {
            p->state       = PARSE_WAIT_LEN;
            p->payload_idx = 0;
            p->running_xor = 0;
        }
        /* Non-SYNC bytes between frames are silently discarded. */
        return FRAME_INCOMPLETE;

    case PARSE_WAIT_LEN:
        if (byte == 0 || byte > FRAME_MAX_PAYLOAD) {
            /* Invalid LEN: re-arm and report error. */
            p->state = PARSE_WAIT_SYNC;
            return FRAME_ERROR;
        }
        p->payload_len = byte;
        p->running_xor = byte;          /* LEN participates in checksum */
        p->state       = PARSE_WAIT_PAYLOAD;
        return FRAME_INCOMPLETE;

    case PARSE_WAIT_PAYLOAD:
        /* A SYNC byte here is just data — do NOT re-sync inside a payload. */
        p->payload[p->payload_idx] = byte;
        p->payload_idx             = (uint8_t)(p->payload_idx + 1);
        p->running_xor             = (uint8_t)(p->running_xor ^ byte);
        if (p->payload_idx == p->payload_len) {
            p->state = PARSE_WAIT_CHECKSUM;
        }
        return FRAME_INCOMPLETE;

    case PARSE_WAIT_CHECKSUM: {
        bool ok = (byte == p->running_xor);
        p->state = PARSE_WAIT_SYNC;     /* re-arm in either case */
        return ok ? FRAME_READY : FRAME_ERROR;
    }

    default:
        /* Unreachable; defensively re-arm. */
        p->state = PARSE_WAIT_SYNC;
        return FRAME_ERROR;
    }
}

uint8_t frame_parser_payload_len(const frame_parser_t *p) {
    return p->payload_len;
}

const uint8_t *frame_parser_payload(const frame_parser_t *p) {
    return p->payload;
}

uint8_t frame_compute_checksum(const uint8_t *data, uint8_t len) {
    uint8_t xor = 0;
    for (uint8_t i = 0; i < len; i++) {
        xor = (uint8_t)(xor ^ data[i]);
    }
    return xor;
}
```

## What an interviewer is looking for

- **Explicit state machine.** A `switch` on a `parser_state_t` is clearer than a tangle of `if`s. Each case handles exactly one byte's worth of work and either stays or transitions to one specific next state. Senior reviewers will glance at this first.
- **Validate `LEN` before writing to `payload[]`.** This is the single most important embedded-security check in the whole problem. If `byte > FRAME_MAX_PAYLOAD` is accepted, the next `LEN` writes past the end of the buffer and corrupts the rest of `frame_parser_t` (and the stack). The check belongs *in* `WAIT_LEN`, before the transition.
- **Streaming checksum, not buffered.** The XOR accumulates one byte at a time as the payload arrives. There is no second pass over `payload[]` at the end. In a real UART RX ISR, a second pass is unaffordable; the same idiom transfers cleanly to CRCs.
- **Auto-resync on every exit.** `FRAME_READY` and `FRAME_ERROR` both put the parser back in `WAIT_SYNC`. There is no separate "error" state to manually clear, and no `frame_parser_reset` call required between frames. The state machine is self-cleaning.
- **A `0x7E` byte in the payload is data.** The parser is in `WAIT_PAYLOAD` when it arrives, so it gets stored, not interpreted. Special-casing this is the single most common bug — and a sign the candidate hasn't internalised that the state machine, not the byte value, is the source of meaning.
- **Exact-width types throughout.** `uint8_t` everywhere; `(uint8_t)` casts on intermediate XOR/increment expressions to keep `-Wconversion`-style tooling happy and to make the 16-bit-MCU intent obvious.
- **`bool` from `<stdbool.h>` for two-valued state**, not `int`. `frame_parser_t::state` is an `enum` (named, not `int`), which lets the compiler help with switch-coverage warnings.

## Common mistakes worth flagging

1. **Special-casing SYNC in `WAIT_PAYLOAD`.** "If I see another `0x7E`, I'd better restart." No — the state machine is the source of truth, the byte value isn't. The fix is to delete the special case.
2. **Forgetting that `LEN` is part of the checksum.** XOR-folding starts at `LEN`, not at the first payload byte. Half of all first-pass attempts forget this and the test for the clean frame fails by a single bit.
3. **Validating `LEN` *after* the WAIT_PAYLOAD transition** — i.e., letting it write past `payload[FRAME_MAX_PAYLOAD - 1]` and only noticing on the checksum byte. This is a real-world buffer-overrun primitive on any malicious wire.
4. **Returning `FRAME_READY` from a state other than `WAIT_CHECKSUM`.** READY can only be the verdict on the last byte of a complete frame. Any other path is wrong.
5. **Leaving `payload_idx` non-zero on entry to `WAIT_PAYLOAD`.** The cleanest place to zero it is on the SYNC-accepting transition (`WAIT_SYNC → WAIT_LEN`), not on the `LEN → WAIT_PAYLOAD` transition. Either is correct as long as it's consistent — but forgetting both is a common bug that only shows up on the second back-to-back frame.
