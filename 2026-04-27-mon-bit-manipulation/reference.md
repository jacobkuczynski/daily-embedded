# Reference solution — Monday 2026-04-27

Canonical solution. Diff this against your own attempt — the deltas in idiom (build the mask once, explicit `!= 0`, wider type for full-width masks, exact-width types) are the parts an embedded interviewer would call out.

## `src/sensor.c`

```c
#include "sensor.h"

bool status_data_ready(uint8_t status) {
    return (status & (1U << STATUS_DRDY_BIT)) != 0;
}

bool status_has_error(uint8_t status) {
    const uint8_t error_mask = (uint8_t)((1U << STATUS_ERROR_BIT)
                                       | (1U << STATUS_TEMP_OVF_BIT)
                                       | (1U << STATUS_HUM_OVF_BIT));
    return (status & error_mask) != 0;
}

uint8_t status_alert_level(uint8_t status) {
    const uint8_t mask = (uint8_t)((1U << STATUS_ALERT_WIDTH) - 1U);
    return (uint8_t)((status >> STATUS_ALERT_SHIFT) & mask);
}

uint16_t config_set_field(uint16_t reg, uint16_t value,
                          uint8_t shift, uint8_t width) {
    /* Build the mask in a type that has headroom for width == 16. */
    const uint32_t mask = (1UL << width) - 1UL;
    const uint32_t v    = (uint32_t)value & mask;
    return (uint16_t)((reg & ~(mask << shift)) | (v << shift));
}

uint16_t config_clear_field(uint16_t reg, uint8_t shift, uint8_t width) {
    const uint32_t mask = (1UL << width) - 1UL;
    return (uint16_t)(reg & ~(mask << shift));
}
```

## What an interviewer is looking for

- **Use exact-width types and the `#define`s from the header**, not magic numbers like `0x80`. Names beat literals.
- **`1U` not `1`** for shifts — habit-forming, even where this problem doesn't strictly need it.
- **`status_has_error` builds the mask once.** OR-ing bits at compile time is free; the runtime cost is one AND. Testing each bit separately (three branches) is what a junior writes; the mask form is what gets nodded at.
- **`config_set_field` masks `value` *before* shifting it.** This is the single most common bug — an oversized `value` clobbering its neighbors. Tests are written specifically to catch it.
- **The mask is computed in `uint32_t`.** At `width == 16`, `(1U << 16) - 1` on a 16-bit `int` platform is undefined or wraps. Promoting to `uint32_t` (or `unsigned long`) gives you headroom and makes the code portable to small MCUs without changes.
- **One `return`, no early exits.** Personal taste, but in driver code with bit-level operations it's easier to audit when the transformation reads as a single expression.

## Variations you might be asked

- "What if the register is volatile, memory-mapped at a fixed address?" — you'd take a `volatile uint16_t *reg` pointer, do read-modify-write, and now you have to think about whether interrupts can fire mid-write. (That's Tuesday's territory.)
- "Width could go up to 32." — promote everything to `uint64_t` for the mask, or guard `width == 32` separately since `1UL << 32` is UB on 32-bit `unsigned long`.
- "Make this branchless and constant-time." — these implementations already are. Worth pointing that out.

## Common mistakes worth flagging

1. `return status & 0x80;` for `status_data_ready` — works in C because non-zero is truthy, but the return type is `bool`, and explicit `!= 0` is cleaner and survives a future change to `int`.
2. `return ((status >> 1) & 0x07);` — correct, but the magic numbers `1` and `0x07` should be `STATUS_ALERT_SHIFT` and `(1 << STATUS_ALERT_WIDTH) - 1`. The header gave them to you.
3. Forgetting to mask `value` in `config_set_field`. Tests will catch it; an interviewer will catch it faster.
4. Computing `mask` in a too-narrow type and getting bitten at `width == 16`.
