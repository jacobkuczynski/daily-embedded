#include <stdint.h>
#include <stdio.h>
#include "src/sensor.h"

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

static void test_data_ready(void) {
    CHECK(status_data_ready(0x80) == true);   /* DRDY only */
    CHECK(status_data_ready(0x00) == false);  /* nothing */
    CHECK(status_data_ready(0x7F) == false);  /* everything except DRDY */
    CHECK(status_data_ready(0xFF) == true);   /* all bits set */
}

static void test_has_error(void) {
    CHECK(status_has_error(0x00) == false);   /* nothing */
    CHECK(status_has_error(0x10) == true);    /* ERROR only */
    CHECK(status_has_error(0x40) == true);    /* TEMP_OVF only */
    CHECK(status_has_error(0x20) == true);    /* HUM_OVF only */
    CHECK(status_has_error(0x70) == true);    /* all three */
    CHECK(status_has_error(0x81) == false);   /* DRDY + BUSY only */
    CHECK(status_has_error(0x0E) == false);   /* alert level set, no error */
    CHECK(status_has_error(0x8F) == false);   /* DRDY + alert + BUSY, no error bits */
}

static void test_alert_level(void) {
    /* Every level 0..7 should round-trip cleanly. */
    for (uint8_t lvl = 0; lvl < 8; lvl++) {
        uint8_t status = (uint8_t)(lvl << 1);
        CHECK_EQ_U(status_alert_level(status), lvl);
    }
    /* Surrounding bits must not bleed in. */
    CHECK_EQ_U(status_alert_level(0xF1), 0);  /* alert = 0 with neighbors set */
    CHECK_EQ_U(status_alert_level(0xFF), 7);  /* alert = 7 */
    CHECK_EQ_U(status_alert_level(0xF3), 1);  /* alert = 001 */
    CHECK_EQ_U(status_alert_level(0xFD), 6);  /* alert = 110 */
}

static void test_config_set_field(void) {
    /* Single-bit field at position 0 */
    CHECK_EQ_U(config_set_field(0x0000, 1, 0, 1), 0x0001);
    CHECK_EQ_U(config_set_field(0xFFFE, 1, 0, 1), 0xFFFF);
    CHECK_EQ_U(config_set_field(0xFFFF, 0, 0, 1), 0xFFFE);

    /* 4-bit field in the middle */
    CHECK_EQ_U(config_set_field(0x0000, 0xA, 4, 4), 0x00A0);
    CHECK_EQ_U(config_set_field(0xFF0F, 0xA, 4, 4), 0xFFAF);

    /* Oversized value must be masked, not corrupt neighbors */
    CHECK_EQ_U(config_set_field(0x0000, 0xFFFF, 4, 4), 0x00F0);
    CHECK_EQ_U(config_set_field(0xAA55, 0xFFFF, 4, 4), 0xAAF5);

    /* Whole register */
    CHECK_EQ_U(config_set_field(0x0000, 0x1234, 0, 16), 0x1234);
    CHECK_EQ_U(config_set_field(0xFFFF, 0x1234, 0, 16), 0x1234);

    /* Top of register */
    CHECK_EQ_U(config_set_field(0x0000, 0x3, 14, 2), 0xC000);
    CHECK_EQ_U(config_set_field(0x3FFF, 0x0, 14, 2), 0x3FFF);
}

static void test_config_clear_field(void) {
    CHECK_EQ_U(config_clear_field(0xFFFF, 0, 1), 0xFFFE);
    CHECK_EQ_U(config_clear_field(0xFFFF, 4, 4), 0xFF0F);
    CHECK_EQ_U(config_clear_field(0xFFFF, 0, 16), 0x0000);
    CHECK_EQ_U(config_clear_field(0x1234, 0, 4), 0x1230);
    CHECK_EQ_U(config_clear_field(0x0000, 4, 4), 0x0000);
    CHECK_EQ_U(config_clear_field(0xFFFF, 14, 2), 0x3FFF);
    CHECK_EQ_U(config_clear_field(0xAAAA, 4, 4), 0xAA0A);
}

int main(void) {
    test_data_ready();
    test_has_error();
    test_alert_level();
    test_config_set_field();
    test_config_clear_field();

    int passed = tests_run - tests_failed;
    printf("\n%d/%d tests passed\n", passed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
