#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "src/frame_parser.h"

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(expr) do {                                                   \
    tests_run++;                                                           \
    if (!(expr)) {                                                         \
        tests_failed++;                                                    \
        fprintf(stderr, "FAIL  line %d: %s\n", __LINE__, #expr);           \
    }                                                                      \
} while (0)

#define CHECK_EQ_U(actual, expected) do {                                  \
    tests_run++;                                                           \
    unsigned long _a = (unsigned long)(actual);                            \
    unsigned long _e = (unsigned long)(expected);                          \
    if (_a != _e) {                                                        \
        tests_failed++;                                                    \
        fprintf(stderr,                                                    \
                "FAIL  line %d: %s == %s  (got 0x%lx, expected 0x%lx)\n",  \
                __LINE__, #actual, #expected, _a, _e);                     \
    }                                                                      \
} while (0)

/* Feed a buffer byte by byte and return the status from the LAST byte. */
static frame_status_t feed_all(frame_parser_t *p, const uint8_t *bytes, uint8_t n) {
    frame_status_t s = FRAME_INCOMPLETE;
    for (uint8_t i = 0; i < n; i++) {
        s = frame_parser_feed(p, bytes[i]);
    }
    return s;
}

/* Build a valid wire frame in `out`, return total length on the wire. */
static uint8_t build_frame(uint8_t *out, const uint8_t *payload, uint8_t len) {
    out[0] = FRAME_SYNC;
    out[1] = len;
    for (uint8_t i = 0; i < len; i++) {
        out[2 + i] = payload[i];
    }
    /* Checksum is XOR over LEN + payload, identical to xor of out[1..1+len]. */
    out[2 + len] = frame_compute_checksum(&out[1], (uint8_t)(len + 1));
    return (uint8_t)(len + 3);
}

static void test_helper_checksum(void) {
    /* XOR identity: empty buffer => 0 */
    CHECK_EQ_U(frame_compute_checksum((const uint8_t *)"", 0), 0);

    /* Single byte: returns the byte itself. */
    uint8_t one[] = { 0xA5 };
    CHECK_EQ_U(frame_compute_checksum(one, 1), 0xA5);

    /* Pair that cancels. */
    uint8_t pair[] = { 0xA5, 0xA5 };
    CHECK_EQ_U(frame_compute_checksum(pair, 2), 0x00);

    /* Multi-byte. */
    uint8_t multi[] = { 0x01, 0x02, 0x04, 0x08 };
    CHECK_EQ_U(frame_compute_checksum(multi, 4), 0x0F);
}

static void test_clean_frame(void) {
    frame_parser_t p;
    frame_parser_reset(&p);

    uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    uint8_t wire[16];
    uint8_t total = build_frame(wire, payload, 4);

    /* Every byte except the last should be INCOMPLETE; the last should be READY. */
    for (uint8_t i = 0; i < total - 1; i++) {
        CHECK_EQ_U(frame_parser_feed(&p, wire[i]), FRAME_INCOMPLETE);
    }
    CHECK_EQ_U(frame_parser_feed(&p, wire[total - 1]), FRAME_READY);

    /* Payload survives intact. */
    CHECK_EQ_U(frame_parser_payload_len(&p), 4);
    const uint8_t *got = frame_parser_payload(&p);
    CHECK(got != 0);
    if (got != 0) {
        CHECK_EQ_U(got[0], 0xDE);
        CHECK_EQ_U(got[1], 0xAD);
        CHECK_EQ_U(got[2], 0xBE);
        CHECK_EQ_U(got[3], 0xEF);
    }
}

static void test_garbage_then_frame(void) {
    frame_parser_t p;
    frame_parser_reset(&p);

    /* Random non-SYNC bytes — every one should be silently discarded. */
    uint8_t garbage[] = { 0x00, 0xFF, 0x12, 0x34, 0xAA };
    for (uint8_t i = 0; i < (uint8_t)sizeof(garbage); i++) {
        CHECK_EQ_U(frame_parser_feed(&p, garbage[i]), FRAME_INCOMPLETE);
    }

    /* Now feed a valid frame; it should parse cleanly. */
    uint8_t payload[] = { 0x42 };
    uint8_t wire[8];
    uint8_t total = build_frame(wire, payload, 1);
    CHECK_EQ_U(feed_all(&p, wire, total), FRAME_READY);
    CHECK_EQ_U(frame_parser_payload_len(&p), 1);
    CHECK_EQ_U(frame_parser_payload(&p)[0], 0x42);
}

static void test_bad_checksum(void) {
    frame_parser_t p;
    frame_parser_reset(&p);

    /* SYNC, LEN=2, payload {0x01, 0x02}, correct chk = 2^1^2 = 1, send 0xFF instead. */
    uint8_t bad[] = { FRAME_SYNC, 0x02, 0x01, 0x02, 0xFF };
    /* All but last are INCOMPLETE. */
    for (uint8_t i = 0; i < 4; i++) {
        CHECK_EQ_U(frame_parser_feed(&p, bad[i]), FRAME_INCOMPLETE);
    }
    /* Checksum byte fires the error. */
    CHECK_EQ_U(frame_parser_feed(&p, bad[4]), FRAME_ERROR);
}

static void test_invalid_len(void) {
    frame_parser_t p;
    frame_parser_reset(&p);

    /* LEN = 0 is invalid. */
    CHECK_EQ_U(frame_parser_feed(&p, FRAME_SYNC), FRAME_INCOMPLETE);
    CHECK_EQ_U(frame_parser_feed(&p, 0x00), FRAME_ERROR);

    /* LEN > MAX is invalid. */
    frame_parser_reset(&p);
    CHECK_EQ_U(frame_parser_feed(&p, FRAME_SYNC), FRAME_INCOMPLETE);
    CHECK_EQ_U(frame_parser_feed(&p, (uint8_t)(FRAME_MAX_PAYLOAD + 1)), FRAME_ERROR);
}

static void test_back_to_back(void) {
    frame_parser_t p;
    frame_parser_reset(&p);

    uint8_t pa[] = { 0x11, 0x22 };
    uint8_t pb[] = { 0x33, 0x44, 0x55 };
    uint8_t wa[8], wb[8];
    uint8_t na = build_frame(wa, pa, 2);
    uint8_t nb = build_frame(wb, pb, 3);

    CHECK_EQ_U(feed_all(&p, wa, na), FRAME_READY);
    CHECK_EQ_U(frame_parser_payload_len(&p), 2);
    CHECK_EQ_U(frame_parser_payload(&p)[1], 0x22);

    /* No reset — parser should already be re-armed. */
    CHECK_EQ_U(feed_all(&p, wb, nb), FRAME_READY);
    CHECK_EQ_U(frame_parser_payload_len(&p), 3);
    CHECK_EQ_U(frame_parser_payload(&p)[0], 0x33);
    CHECK_EQ_U(frame_parser_payload(&p)[2], 0x55);
}

static void test_payload_contains_sync(void) {
    frame_parser_t p;
    frame_parser_reset(&p);

    /* Payload literally contains the SYNC byte 0x7E.
     * The parser is in WAIT_PAYLOAD when it sees it, so it must store
     * it as data and NOT treat it as a new start-of-frame.            */
    uint8_t payload[] = { 0x7E, 0x01, 0x7E };
    uint8_t wire[8];
    uint8_t total = build_frame(wire, payload, 3);

    CHECK_EQ_U(feed_all(&p, wire, total), FRAME_READY);
    CHECK_EQ_U(frame_parser_payload_len(&p), 3);
    const uint8_t *got = frame_parser_payload(&p);
    CHECK_EQ_U(got[0], 0x7E);
    CHECK_EQ_U(got[1], 0x01);
    CHECK_EQ_U(got[2], 0x7E);
}

static void test_recovery_after_error(void) {
    frame_parser_t p;
    frame_parser_reset(&p);

    /* Feed a bad-checksum frame first. */
    uint8_t bad[] = { FRAME_SYNC, 0x01, 0xAA, 0x00 /* wrong chk; correct would be 0xAB */ };
    CHECK_EQ_U(feed_all(&p, bad, (uint8_t)sizeof(bad)), FRAME_ERROR);

    /* Parser should be re-armed and ready to parse the next clean frame. */
    uint8_t payload[] = { 0x99 };
    uint8_t wire[8];
    uint8_t total = build_frame(wire, payload, 1);
    CHECK_EQ_U(feed_all(&p, wire, total), FRAME_READY);
    CHECK_EQ_U(frame_parser_payload_len(&p), 1);
    CHECK_EQ_U(frame_parser_payload(&p)[0], 0x99);
}

int main(void) {
    test_helper_checksum();
    test_clean_frame();
    test_garbage_then_frame();
    test_bad_checksum();
    test_invalid_len();
    test_back_to_back();
    test_payload_contains_sync();
    test_recovery_after_error();

    int passed = tests_run - tests_failed;
    printf("\n%d/%d tests passed\n", passed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
