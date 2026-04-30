# Hints — read one at a time

## Hint 1 — three primitives cover everything

Almost all of bit-bashing reduces to three patterns:

- **Test a bit:** `(reg & (1U << bit)) != 0`
- **Set a field:** `reg | (value << shift)`
- **Clear a field:** `reg & ~(mask << shift)`

The `1U` (unsigned) matters in general — shifting `1 << 31` into a signed `int` is undefined behavior. Even though the bits in this problem don't go that high, get into the habit now.

For `status_has_error`, build a single mask of "all the error bits" once and AND once.

## Hint 2 — width-N masks need headroom

`config_set_field` and `config_clear_field` both need a mask of `width` ones. The textbook expression is:

```c
mask = (1U << width) - 1;
```

This is fine for `width < 16`. At `width == 16` it works on platforms where `unsigned int` is 32-bit (which yours is), but it is *not* portable to 16-bit `int` platforms. The robust pattern is to compute the mask in a wider type:

```c
uint32_t mask = (1UL << width) - 1UL;
```

Then cast back to `uint16_t` at the end. This also matches the constraint in the problem about the code working on a 16-bit MCU.

## Hint 3 — set_field's two-step shape

A clean implementation looks like this:

```c
uint32_t mask = (1UL << width) - 1UL;
value &= mask;                                /* mask oversized inputs */
reg   &= (uint16_t)~(mask << shift);          /* clear the destination field */
reg   |= (uint16_t)((uint32_t)value << shift); /* OR in the new bits */
return reg;
```

`config_clear_field` is the same shape minus the OR step.

The order matters: mask the value *before* shifting it into place, otherwise an oversized `value` will corrupt the neighbors you just preserved.
