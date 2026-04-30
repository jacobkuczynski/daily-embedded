#include "frame_parser.h"

void frame_parser_reset(frame_parser_t *p) {
    /* TODO: put the parser into the initial state.
     *       state = PARSE_WAIT_SYNC, counters and running_xor zeroed.
     *       payload[] does not need to be zeroed.
     */
    (void)p;
}

frame_status_t frame_parser_feed(frame_parser_t *p, uint8_t byte) {
    /* TODO: drive the state machine forward by one byte and return
     *       the appropriate frame_status_t.
     *
     *       States and transitions:
     *         WAIT_SYNC      : byte == FRAME_SYNC      -> WAIT_LEN
     *                          else                    -> stay (INCOMPLETE)
     *         WAIT_LEN       : byte in [1, MAX]        -> WAIT_PAYLOAD
     *                          else                    -> WAIT_SYNC (ERROR)
     *         WAIT_PAYLOAD   : store byte; on last     -> WAIT_CHECKSUM
     *         WAIT_CHECKSUM  : byte == running_xor     -> WAIT_SYNC (READY)
     *                          else                    -> WAIT_SYNC (ERROR)
     */
    (void)p;
    (void)byte;
    return FRAME_INCOMPLETE;
}

uint8_t frame_parser_payload_len(const frame_parser_t *p) {
    /* TODO: return the declared LEN of the most recently completed frame. */
    (void)p;
    return 0;
}

const uint8_t *frame_parser_payload(const frame_parser_t *p) {
    /* TODO: return a pointer to p->payload (read-only view).
     *       Stub returns the buffer pointer so tests can call this without
     *       segfaulting; payload_len() still returns 0 so callers stop early. */
    return p->payload;
}

uint8_t frame_compute_checksum(const uint8_t *data, uint8_t len) {
    /* TODO: XOR-fold data[0..len-1]. Return 0 when len == 0. */
    (void)data;
    (void)len;
    return 0;
}
