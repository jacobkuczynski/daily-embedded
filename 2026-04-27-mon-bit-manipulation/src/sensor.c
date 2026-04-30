#include "sensor.h"

bool status_data_ready(uint8_t status) {
    /* TODO: return true if DRDY (bit 7) is set */
    (void)status;
    return false;
}

bool status_has_error(uint8_t status) {
    /* TODO: return true if any of ERROR (bit 4), TEMP_OVF (bit 6),
     *       or HUM_OVF (bit 5) is set. */
    (void)status;
    return false;
}

uint8_t status_alert_level(uint8_t status) {
    /* TODO: extract bits 3..1 as a 0..7 value. Surrounding bits must
     *       not influence the result. */
    (void)status;
    return 0;
}

uint16_t config_set_field(uint16_t reg, uint16_t value, uint8_t shift, uint8_t width) {
    /* TODO: write `value` into the [shift, shift+width) field of `reg`.
     *       Mask `value` to `width` bits first so oversized inputs do
     *       not corrupt neighboring fields. Other bits unchanged. */
    (void)reg; (void)value; (void)shift; (void)width;
    return 0;
}

uint16_t config_clear_field(uint16_t reg, uint8_t shift, uint8_t width) {
    /* TODO: zero the field at [shift, shift+width). Other bits unchanged. */
    (void)reg; (void)shift; (void)width;
    return 0;
}
