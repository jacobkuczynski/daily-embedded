# Hints

Read these one at a time. Try to make progress on your own first.

---

## Hint 1 — the width-promotion trap

In C, when you do `buf[0] << 8`, the operand is promoted to `int` first. On a 16-bit MCU, `int` is 16 bits, so the high bits of that shifted value can be lost — and `buf[0] << 24` is outright undefined behavior at that width.

The fix is mechanical: cast the byte to a wider unsigned type **before** the shift.

```c
uint16_t hi = (uint16_t)buf[0] << 8;     // for 16-bit results
uint32_t hi = (uint32_t)buf[0] << 24;    // for 32-bit results
```

This cast is also what tells a future reader "I know what `int` is, and I picked the destination width on purpose."

---

## Hint 2 — sign extension for the 24-bit read

A 24-bit signed value isn't a native C type, so you have to sign-extend it yourself. The pattern is:

1. Build the 24-bit value as `uint32_t` first (clean, no sign trouble yet).
2. Check whether bit 23 is set.
3. If it is, OR in `0xFF000000` to fill the upper byte with 1s.
4. Cast the resulting `uint32_t` to `int32_t`.

The cast in step 4 is implementation-defined for values that don't fit in the signed range, but in practice every C99 compiler does what you want here — a bit pattern reinterpretation. If you want to avoid even that, use `memcpy` to copy the `uint32_t` bits into an `int32_t`.

---

## Hint 3 — bswap32 in one line, pack_le32 in four

`bswap32` is just four shifts ORed together. Do it in a single expression so the structure is visible:

```c
return ((x & 0xFF000000UL) >> 24) |
       ((x & 0x00FF0000UL) >>  8) |
       ((x & 0x0000FF00UL) <<  8) |
       ((x & 0x000000FFUL) << 24);
```

`pack_le32` is four byte-stores. Mask each with `0xFF` so the high bytes don't survive the conversion to `uint8_t` — the conversion would do it for you, but the mask documents intent and silences pickier warnings:

```c
buf[0] = (uint8_t)(value        & 0xFFU);
buf[1] = (uint8_t)((value >>  8) & 0xFFU);
buf[2] = (uint8_t)((value >> 16) & 0xFFU);
buf[3] = (uint8_t)((value >> 24) & 0xFFU);
```
