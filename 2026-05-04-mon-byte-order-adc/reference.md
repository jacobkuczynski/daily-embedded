# Reference — 2026-05-04 — Byte Order: 24-bit ADC samples and multi-byte buffers

## Canonical solution

```c
#include "byteorder.h"

uint16_t read_be16(const uint8_t *buf) {
    /* Cast before shift so the shift happens at uint16_t width.
     * On a 16-bit MCU, an unguarded `buf[0] << 8` is still safe, but
     * being explicit makes intent obvious and survives copy/paste
     * into a 32-bit context. */
    return (uint16_t)((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

uint16_t read_le16(const uint8_t *buf) {
    return (uint16_t)((uint16_t)buf[1] << 8) | (uint16_t)buf[0];
}

int32_t read_be24_signed(const uint8_t *buf) {
    /* Build the 24-bit value as uint32_t — promotion to int alone
     * would be unsafe on a 16-bit MCU when shifting by 16. */
    uint32_t u = ((uint32_t)buf[0] << 16) |
                 ((uint32_t)buf[1] <<  8) |
                  (uint32_t)buf[2];

    /* Sign-extend bit 23: if it's set, fill the top byte with 1s. */
    if (u & 0x00800000UL) {
        u |= 0xFF000000UL;
    }

    /* uint32_t -> int32_t reinterprets the bit pattern on every
     * compiler we care about. (memcpy would be the strictly portable
     * way; this is the conventional one.) */
    return (int32_t)u;
}

uint32_t bswap32(uint32_t x) {
    return ((x & 0xFF000000UL) >> 24) |
           ((x & 0x00FF0000UL) >>  8) |
           ((x & 0x0000FF00UL) <<  8) |
           ((x & 0x000000FFUL) << 24);
}

void pack_le32(uint8_t *buf, uint32_t value) {
    buf[0] = (uint8_t)( value        & 0xFFU);
    buf[1] = (uint8_t)((value >>  8) & 0xFFU);
    buf[2] = (uint8_t)((value >> 16) & 0xFFU);
    buf[3] = (uint8_t)((value >> 24) & 0xFFU);
}
```

## What an interviewer is looking for

- **Width promotion awareness.** The candidate should explicitly cast bytes to a wider unsigned type (`uint16_t` or `uint32_t`) *before* shifting, with a clear understanding of why: on a 16-bit MCU, the C integer-promotion rules promote `uint8_t` to `int` (16-bit), and shifting by 16 or more is undefined behavior. On a 32-bit host the bug is dormant, which makes it a classic field-failure pattern.
- **Sign extension done deliberately.** For `read_be24_signed`, the interview-worthy answer builds the value in a `uint32_t`, tests bit 23 explicitly, and OR-fills the top byte with `0xFF000000`. The dangerous shortcut — letting an `int32_t` cast "do the right thing" — does not work for a 24-bit value, since C only sign-extends from native widths.
- **Exact-width types.** Returning `uint16_t` and `uint32_t` (not `unsigned`, not `int`) — and using `uint8_t` for buffer bytes — signals the candidate understands that on embedded targets, integer width is part of the contract, not an implementation detail.
- **`UL` suffixes on 32-bit constants.** `0x00800000UL` and `0xFF000000UL` make explicit that the constant is `unsigned long`, which is at least 32 bits everywhere. Without the suffix, `0xFF000000` is `unsigned int` on a 32-bit compiler but undefined-or-implementation-defined on a 16-bit one.
- **`uint32_t → int32_t` cast as the single point of representation conversion.** Strong candidates know this isn't strictly portable C99 (the cast is implementation-defined for out-of-range values), and may volunteer `memcpy` as the strictly correct alternative. Either answer is fine; surfacing the awareness is the signal.
- **No magic numbers in `bswap32`.** The four-line shift-and-OR with explicit `0xFF` masks is more readable and more obviously correct than a "clever" rotate-based version.

## Common mistakes worth flagging

1. **Ignoring the 16-bit MCU constraint.** Writing `(buf[0] << 24) | (buf[1] << 16) | ...` without a cast. Compiles fine on a 32-bit host; undefined behavior on a 16-bit target. The fix is `(uint32_t)buf[0] << 24`.
2. **"Casting at the end" for sign extension.** `(int32_t)((buf[0] << 16) | (buf[1] << 8) | buf[2])` does NOT sign-extend a 24-bit value — the high byte is zero, so a "negative" sample reads back as a large positive `int32_t`. The sign-fill of `0xFF000000` is not optional.
3. **Forgetting the `UL` suffix on 32-bit literals.** `0xFF000000` without a suffix is implementation-defined on a 16-bit MCU. `0xFF000000UL` is portable.
4. **Endianness confusion in `pack_le32`.** Putting the MSB in `buf[0]` instead of the LSB. The naming convention helps: "LE" = little end (the LSB) goes first.
5. **Missing the `uint8_t` cast in `pack_le32` stores.** `buf[i] = value >> 8` works on most compilers because the implicit narrowing conversion truncates, but stricter warning levels (`-Wconversion`) flag it. The explicit `(uint8_t)(... & 0xFFU)` is self-documenting and warning-clean.
