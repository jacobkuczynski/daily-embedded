#ifndef FRAME_PARSER_H
#define FRAME_PARSER_H

#include <stdint.h>
#include <stdbool.h>

/* Wire-protocol constants */
#define FRAME_SYNC          ((uint8_t)0x7E)
#define FRAME_MAX_PAYLOAD   ((uint8_t)32)

/* Status returned by frame_parser_feed() for each byte. */
typedef enum {
    FRAME_INCOMPLETE = 0,   /* still assembling, no event yet  */
    FRAME_READY      = 1,   /* this byte completed a good frame */
    FRAME_ERROR      = 2,   /* invalid LEN or bad checksum     */
} frame_status_t;

/* Internal state machine. Exposed only so the parser struct is sized at compile time. */
typedef enum {
    PARSE_WAIT_SYNC      = 0,
    PARSE_WAIT_LEN       = 1,
    PARSE_WAIT_PAYLOAD   = 2,
    PARSE_WAIT_CHECKSUM  = 3,
} parser_state_t;

typedef struct {
    parser_state_t state;
    uint8_t        payload[FRAME_MAX_PAYLOAD];
    uint8_t        payload_len;   /* declared LEN of the current/completed frame */
    uint8_t        payload_idx;   /* bytes received so far inside payload[]      */
    uint8_t        running_xor;   /* running XOR of LEN + payload bytes seen     */
} frame_parser_t;

/**
 * @brief  Reset the parser to its initial state (waiting for SYNC).
 *         Must be called once before the first feed().
 */
void frame_parser_reset(frame_parser_t *p);

/**
 * @brief  Advance the parser by one received byte.
 *
 *         Returns FRAME_READY on the byte that completes a valid frame.
 *         Returns FRAME_ERROR on a byte that violates the protocol
 *         (LEN out of range, checksum mismatch); the parser re-arms
 *         itself to look for the next SYNC.
 *         Returns FRAME_INCOMPLETE otherwise.
 */
frame_status_t frame_parser_feed(frame_parser_t *p, uint8_t byte);

/**
 * @brief  Length (in bytes) of the most recently completed frame's payload.
 *         Valid only immediately after frame_parser_feed() returned FRAME_READY,
 *         i.e. before the next feed() that begins a new payload.
 */
uint8_t frame_parser_payload_len(const frame_parser_t *p);

/**
 * @brief  Pointer to the most recently completed frame's payload bytes.
 *         Length is given by frame_parser_payload_len(). Same validity
 *         window as that function.
 */
const uint8_t *frame_parser_payload(const frame_parser_t *p);

/**
 * @brief  XOR of data[0..len-1]. Returns 0 when len == 0 (XOR identity).
 *         Used by the TX path to build a frame and (conceptually) by the
 *         RX state machine to verify one.
 */
uint8_t frame_compute_checksum(const uint8_t *data, uint8_t len);

#endif /* FRAME_PARSER_H */
