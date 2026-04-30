# Hints — read one at a time

## Hint 1 — name the four states up front

Byte-driven parsers are easiest to write when the state machine is explicit. Sketch it on paper before you start typing:

```
WAIT_SYNC    --(byte == 0x7E)-->  WAIT_LEN
WAIT_LEN     --(1..MAX)-->        WAIT_PAYLOAD     (record LEN)
                  --(else)-->     WAIT_SYNC        (return ERROR)
WAIT_PAYLOAD --(byte stored)-->   WAIT_PAYLOAD     (until we have LEN bytes)
                  --(LEN-th)-->   WAIT_CHECKSUM
WAIT_CHECKSUM--(matches)-->       WAIT_SYNC        (return READY)
                  --(else)-->     WAIT_SYNC        (return ERROR)
```

Note that `FRAME_ERROR` and `FRAME_READY` *both* leave the parser in `WAIT_SYNC` — there is no separate "error" state to clear. The auto-resync is the whole point.

## Hint 2 — accumulate the checksum as you go

Don't store the payload, then re-XOR it at the end. Roll the checksum forward one byte at a time:

```c
case PARSE_WAIT_LEN:
    p->payload_len = byte;
    p->payload_idx = 0;
    p->running_xor = byte;        /* LEN is part of the checksum */
    p->state = PARSE_WAIT_PAYLOAD;
    return FRAME_INCOMPLETE;

case PARSE_WAIT_PAYLOAD:
    p->payload[p->payload_idx++] = byte;
    p->running_xor ^= byte;
    if (p->payload_idx == p->payload_len) {
        p->state = PARSE_WAIT_CHECKSUM;
    }
    return FRAME_INCOMPLETE;

case PARSE_WAIT_CHECKSUM: {
    bool ok = (byte == p->running_xor);
    p->state = PARSE_WAIT_SYNC;   /* re-arm either way */
    return ok ? FRAME_READY : FRAME_ERROR;
}
```

That gives you O(1) extra work per byte and matches what real UART drivers do — the ISR has no time to walk a buffer.

## Hint 3 — the SYNC-in-payload trap, and bounds before write

Two specific places where naive parsers break:

1. **A `0x7E` byte inside a payload is just data.** Your `WAIT_PAYLOAD` case must blindly store the byte and increment the index. Only `WAIT_SYNC` interprets `0x7E` as a frame start. If you find yourself adding a special case for `byte == FRAME_SYNC` inside `WAIT_PAYLOAD`, stop — that's the bug.

2. **Validate `LEN` before you write to `payload[]`.** A malicious or noisy stream can give you `LEN = 200`. If you accept it and the next 200 feed-bytes write into `payload[200]`, you've trampled the rest of the parser struct (and the stack). The check is one line: `if (byte == 0 || byte > FRAME_MAX_PAYLOAD) return FRAME_ERROR;` — but it has to happen *in* `WAIT_LEN`, before you transition to `WAIT_PAYLOAD`.
