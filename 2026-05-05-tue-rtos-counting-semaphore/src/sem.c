#include "sem.h"

void sem_init(sem_t *s, uint16_t initial, uint16_t max_count) {
    /* TODO: set max_count, then store initial (clamped to max_count)
     *       into count. Init runs before the object is shared with any
     *       ISR, so no critical section is required. */
    (void)s; (void)initial; (void)max_count;
}

bool sem_post(sem_t *s) {
    /* TODO: enter CS, increment count if count < max_count, exit CS,
     *       return whether the increment happened. Must be ISR-safe. */
    (void)s;
    return false;
}

bool sem_try_take(sem_t *s) {
    /* TODO: enter CS, decrement count if count > 0, exit CS,
     *       return whether the decrement happened. */
    (void)s;
    return false;
}

bool sem_try_take_n(sem_t *s, uint16_t n) {
    /* TODO: atomic all-or-nothing decrement by `n`.
     *       If count >= n, decrement by n and return true.
     *       Otherwise leave count unchanged and return false.
     *       Watch the early-return path: cs_exit must run before
     *       every return. */
    (void)s; (void)n;
    return false;
}

uint16_t sem_count(const sem_t *s) {
    /* TODO: return a snapshot of count, wrapped in a critical section
     *       so the read is atomic on a 16-bit MCU. */
    (void)s;
    return 0;
}
