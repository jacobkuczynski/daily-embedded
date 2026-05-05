#include <stdint.h>
#include <stdio.h>
#include "src/byteorder.h"

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

#define CHECK_EQ_I(actual, expected) do {                                  \
    tests_run++;                                                           \
    long _a = (long)(actual);                                              \
    long _e = (long)(expected);                                            \
    if (_a != _e) {                                                        \
        tests_failed++;                                                    \
        fprintf(stderr,                                                    \
                "FAIL  line %d: %s == %s  (got %ld, expected %ld)\n",      \
                __LINE__, #actual, #expected, _a, _e);                     \
    }                                                                      \
} while (0)

static void test_read_be16(void) {
    uint8_t a[] = {0x00, 0x00};
    uint8_t b[] = {0x12, 0x34};
    uint8_t c[] = {0xFF, 0xFF};
    uint8_t d[] = {0x80, 0x00};
    CHECK_EQ_U(read_be16(a), 0x0000);
    CHECK_EQ_U(read_be16(b), 0x1234);
    CHECK_EQ_U(read_be16(c), 0xFFFF);
    CHECK_EQ_U(read_be16(d), 0x8000);
}

static void test_read_le16(void) {
    uint8_t a[] = {0x00, 0x00};
    uint8_t b[] = {0x34, 0x12};   /* same value as BE {0x12,0x34} */
    uint8_t c[] = {0xFF, 0xFF};
    uint8_t d[] = {0x00, 0x80};
    CHECK_EQ_U(read_le16(a), 0x0000);
    CHECK_EQ_U(read_le16(b), 0x1234);
    CHECK_EQ_U(read_le16(c), 0xFFFF);
    CHECK_EQ_U(read_le16(d), 0x8000);
}

static void test_read_be24_signed(void) {
    /* Positive values */
    uint8_t zero[]   = {0x00, 0x00, 0x00};
    uint8_t one[]    = {0x00, 0x00, 0x01};
    uint8_t mid[]    = {0x12, 0x34, 0x56};
    uint8_t max_pos[] = {0x7F, 0xFF, 0xFF};   /* +8388607 */
    CHECK_EQ_I(read_be24_signed(zero),   0);
    CHECK_EQ_I(read_be24_signed(one),    1);
    CHECK_EQ_I(read_be24_signed(mid),    0x123456);
    CHECK_EQ_I(read_be24_signed(max_pos), 8388607);

    /* Negative values — these are the sign-extension trap */
    uint8_t neg_one[] = {0xFF, 0xFF, 0xFF};   /* -1 */
    uint8_t min_neg[] = {0x80, 0x00, 0x00};   /* -8388608 */
    uint8_t neg_mid[] = {0xFF, 0xFF, 0xFE};   /* -2 */
    uint8_t neg_big[] = {0xC0, 0x00, 0x00};   /* -4194304 */
    CHECK_EQ_I(read_be24_signed(neg_one),  -1);
    CHECK_EQ_I(read_be24_signed(min_neg),  -8388608);
    CHECK_EQ_I(read_be24_signed(neg_mid),  -2);
    CHECK_EQ_I(read_be24_signed(neg_big),  -4194304);

    /* Boundary: bit 23 set vs. clear, all other bits identical-ish */
    uint8_t just_pos[] = {0x7F, 0xFF, 0xFE};   /* +8388606 */
    uint8_t just_neg[] = {0x80, 0x00, 0x01};   /* -8388607 */
    CHECK_EQ_I(read_be24_signed(just_pos),  8388606);
    CHECK_EQ_I(read_be24_signed(just_neg), -8388607);
}

static void test_bswap32(void) {
    CHECK_EQ_U(bswap32(0x00000000UL), 0x00000000UL);
    CHECK_EQ_U(bswap32(0xFFFFFFFFUL), 0xFFFFFFFFUL);
    CHECK_EQ_U(bswap32(0x11223344UL), 0x44332211UL);
    CHECK_EQ_U(bswap32(0xDEADBEEFUL), 0xEFBEADDEUL);
    CHECK_EQ_U(bswap32(0x12345678UL), 0x78563412UL);
    /* Involution: bswap32 is its own inverse */
    CHECK_EQ_U(bswap32(bswap32(0xCAFEBABEUL)), 0xCAFEBABEUL);
    /* Single-byte values exercise the high-shift path */
    CHECK_EQ_U(bswap32(0x000000FFUL), 0xFF000000UL);
    CHECK_EQ_U(bswap32(0xFF000000UL), 0x000000FFUL);
}

static void test_pack_le32(void) {
    uint8_t buf[4];

    pack_le32(buf, 0x00000000UL);
    CHECK_EQ_U(buf[0], 0x00); CHECK_EQ_U(buf[1], 0x00);
    CHECK_EQ_U(buf[2], 0x00); CHECK_EQ_U(buf[3], 0x00);

    pack_le32(buf, 0x11223344UL);
    CHECK_EQ_U(buf[0], 0x44); CHECK_EQ_U(buf[1], 0x33);
    CHECK_EQ_U(buf[2], 0x22); CHECK_EQ_U(buf[3], 0x11);

    pack_le32(buf, 0xDEADBEEFUL);
    CHECK_EQ_U(buf[0], 0xEF); CHECK_EQ_U(buf[1], 0xBE);
    CHECK_EQ_U(buf[2], 0xAD); CHECK_EQ_U(buf[3], 0xDE);

    pack_le32(buf, 0xFFFFFFFFUL);
    CHECK_EQ_U(buf[0], 0xFF); CHECK_EQ_U(buf[1], 0xFF);
    CHECK_EQ_U(buf[2], 0xFF); CHECK_EQ_U(buf[3], 0xFF);

    /* Round-trip: pack LE then read back as LE16 halves. */
    pack_le32(buf, 0x12345678UL);
    CHECK_EQ_U(read_le16(&buf[0]), 0x5678);   /* low half */
    CHECK_EQ_U(read_le16(&buf[2]), 0x1234);   /* high half */
}

int main(void) {
    test_read_be16();
    test_read_le16();
    test_read_be24_signed();
    test_bswap32();
    test_pack_le32();

    int passed = tests_run - tests_failed;
    printf("\n%d/%d tests passed\n", passed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
