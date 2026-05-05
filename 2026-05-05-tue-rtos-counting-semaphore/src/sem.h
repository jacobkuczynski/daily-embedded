#ifndef SEM_H
#define SEM_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Counting semaphore.
 *
 * `count` is mutated from both ISR and thread contexts (via sem_post),
 * so it is `volatile`. `max_count` is set once at init and never
 * changes thereafter, so it does not need to be volatile.
 */
typedef struct {
    volatile uint16_t count;
    uint16_t          max_count;
} sem_t;

/**
 * Critical-section primitives provided by the platform. On a Cortex-M
 * these would translate to PRIMASK save/disable + restore. Tests
 * provide stub implementations that count calls and track nesting depth.
 */
extern void cs_enter(void);
extern void cs_exit(void);

/**
 * @brief  Initialize a semaphore. Init runs before the semaphore is
 *         shared with any ISR, so no critical section is required.
 *
 *         If `initial > max_count`, the count is clamped to `max_count`.
 */
void sem_init(sem_t *s, uint16_t initial, uint16_t max_count);

/**
 * @brief  Atomically increment `count` if `count < max_count`.
 *         Returns true on success, false if the semaphore is at the cap.
 *         Safe to call from ISR or thread context.
 */
bool sem_post(sem_t *s);

/**
 * @brief  Atomically decrement `count` if `count > 0`. Non-blocking.
 *         Returns true on success, false if `count == 0`.
 *         Thread-context call.
 */
bool sem_try_take(sem_t *s);

/**
 * @brief  Atomic all-or-nothing decrement by `n`.
 *         If `count >= n`, decrement by `n` and return true.
 *         Otherwise leave count unchanged and return false.
 *         A partial decrement is NOT acceptable.
 */
bool sem_try_take_n(sem_t *s, uint16_t n);

/**
 * @brief  Return a snapshot of `count`. Wrap the read in a critical
 *         section so the read appears atomic regardless of MCU width.
 */
uint16_t sem_count(const sem_t *s);

#endif /* SEM_H */
