# Monday — Bit Manipulation: Sensor Status Register

**Topic:** Bit manipulation, register access patterns
**Difficulty:** Easy
**Time budget:** 20 minutes
**Date:** 2026-04-27

## Scenario

You're writing a driver for the **TC-HT22**, a fictional I2C temperature/humidity sensor running on a 16-bit microcontroller. Your job is to implement five small bit-level helpers used throughout the driver — three for parsing the 8-bit `STATUS` register, two for safely modifying fields inside the 16-bit `CONFIG` register.

This kind of register-bashing is the bread and butter of bare-metal driver code. Interviewers like it because it surfaces sloppy assumptions about integer width, sign, and operator precedence quickly.

## Register layouts

### `STATUS` (8-bit, read-only)

| Bit | Name | Meaning |
|-----|------|---------|
| 7 | DRDY | New measurement available |
| 6 | TEMP_OVF | Temperature reading overflowed |
| 5 | HUM_OVF | Humidity reading overflowed |
| 4 | ERROR | Generic error flag |
| 3–1 | ALERT_LEVEL | 0 = none, 7 = critical (3-bit field) |
| 0 | BUSY | Sensor is mid-conversion |

### `CONFIG` (16-bit, read/write)

A general-purpose 16-bit configuration register. The driver needs to update arbitrary bit-fields inside it without disturbing the rest. You're implementing the generic helpers, not specific named fields.

## Tasks

Implement the five functions in `src/sensor.c`. Their contracts are documented in `src/sensor.h`.

1. `status_data_ready(status)` — DRDY check.
2. `status_has_error(status)` — true if `ERROR`, `TEMP_OVF`, or `HUM_OVF` is set.
3. `status_alert_level(status)` — extract bits 3–1 as `0..7`.
4. `config_set_field(reg, value, shift, width)` — write `value` into the field at `[shift, shift+width)` without disturbing other bits. If `value` has bits set outside the bottom `width` bits, mask them off.
5. `config_clear_field(reg, shift, width)` — zero the field at `[shift, shift+width)`.

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

Tests in `tests.c` will be red until you implement the functions. They cover the obvious cases plus a few that catch common mistakes (oversized values bleeding into neighbors, full-width fields, shifts at the top of the register).

## Hints

`hints.md` has three progressive hints. Read them one at a time only if you're stuck.

## Reference

`reference.md` lands in this folder the morning after the problem ships. Diff your attempt against it.
