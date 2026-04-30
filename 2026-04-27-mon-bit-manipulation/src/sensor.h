#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <stdbool.h>

/* STATUS register bit positions */
#define STATUS_DRDY_BIT      7
#define STATUS_TEMP_OVF_BIT  6
#define STATUS_HUM_OVF_BIT   5
#define STATUS_ERROR_BIT     4
#define STATUS_ALERT_SHIFT   1
#define STATUS_ALERT_WIDTH   3
#define STATUS_BUSY_BIT      0

/**
 * @brief  Returns true iff DRDY (bit 7) is set.
 */
bool status_data_ready(uint8_t status);

/**
 * @brief  Returns true iff any error flag is set:
 *         ERROR (bit 4), TEMP_OVF (bit 6), or HUM_OVF (bit 5).
 *         Other bits do not affect the result.
 */
bool status_has_error(uint8_t status);

/**
 * @brief  Extracts the 3-bit ALERT_LEVEL field (bits 3..1) as 0..7.
 *         Surrounding bits (DRDY, TEMP_OVF, HUM_OVF, ERROR, BUSY) must
 *         not influence the returned value.
 */
uint8_t status_alert_level(uint8_t status);

/**
 * @brief  Writes `value` into the field at bits [shift, shift+width)
 *         within `reg`, leaving every other bit unchanged.
 *
 *         If `value` has bits set outside the bottom `width` bits,
 *         those extra bits MUST be masked off (they must not corrupt
 *         neighboring fields).
 *
 *         Defined for: shift in [0,15], width in [1,16],
 *         shift + width <= 16.
 */
uint16_t config_set_field(uint16_t reg, uint16_t value, uint8_t shift, uint8_t width);

/**
 * @brief  Zeros the field at bits [shift, shift+width) within `reg`.
 *         Every other bit is unchanged.
 *
 *         Defined for: shift in [0,15], width in [1,16],
 *         shift + width <= 16.
 */
uint16_t config_clear_field(uint16_t reg, uint8_t shift, uint8_t width);

#endif /* SENSOR_H */
