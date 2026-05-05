# Monday — Byte Order: 24-bit ADC samples and multi-byte buffers

**Topic:** Bit manipulation, endianness, sign extension
**Difficulty:** Easy
**Time budget:** 20 minutes
**Date:** 2026-05-04

## Scenario

You're writing the host-side parser for the **TC-ADC32**, a fictional 24-bit delta-sigma ADC connected over SPI. The chip streams sample frames as raw bytes; your parser turns them into the host's native integer types. The same parser also handles a few control-register reads and writes, which use a mix of big-endian and little-endian byte orders depending on the field.

Endianness and sign extension are where bit-level C goes wrong quickly: the bug compiles, the unit test passes on x86, and then the value is silently mangled on a 16-bit target — or worse, a 24-bit signed sample reads back as a huge positive number because nobody sign-extended bit 23.

You're implementing five small helpers used throughout the driver.

## Frame layout

The ADC pushes 3-byte sample frames in **big-endian** byte order. Bytes arrive as `[MSB, MID, LSB]`, and the 24-bit value is signed two's-complement in the range `[-2^23, 2^23 - 1]`.

Control-register reads return 2-byte values, sometimes big-endian (status word) and sometimes little-endian (calibration constant). A 4-byte device serial number is read big-endian and you occasionally need to byte-swap it, and outgoing 32-bit configuration words are written little-endian into a TX buffer.

## Tasks

Implement the five functions in `src/byteorder.c`. Their contracts are documented in `src/byteorder.h`.

1. `read_be16(buf)` — assemble a `uint16_t` from `buf[0]` (MSB) and `buf[1]` (LSB).
2. `read_le16(buf)` — assemble a `uint16_t` from `buf[0]` (LSB) and `buf[1]` (MSB).
3. `read_be24_signed(buf)` — read a 24-bit two's-complement value from `buf[0..2]` (big-endian) and return it sign-extended to `int32_t`.
4. `bswap32(x)` — return `x` with its byte order reversed.
5. `pack_le32(buf, value)` — write `value` into `buf[0..3]` in little-endian order (`buf[0]` = LSB, `buf[3]` = MSB).

## Constraints

- Pure C99.
- No `malloc`, no stdlib functions beyond `<stdint.h>`, `<stdbool.h>`, `<stdio.h>`, `<assert.h>`.
- Code must be correct on a 16-bit MCU — don't assume `int` is 32 bits. In particular, an expression like `buf[0] << 24` is undefined behavior when `int` is 16 bits, because the operand is promoted to `int` before the shift. Cast operands to a wider unsigned type before shifting.
- Use exact-width types (`uint8_t`, `uint16_t`, `uint32_t`, `int32_t`) explicitly.
- Compiles cleanly with `-Wall -Wextra -Wpedantic`.

## Build and test

From this folder:

```
make test
```

Tests in `tests.c` will be red until you implement the functions. They cover the obvious cases plus a few that catch the common traps (sign extension across the 24-bit boundary, 16-bit MCU width promotion, round-trip pack→unpack).

## Hints

`hints.md` has three progressive hints. Read them one at a time only if you're stuck.

## Reference

`reference.md` lands in this folder the morning after the problem ships. Diff your attempt against it.
