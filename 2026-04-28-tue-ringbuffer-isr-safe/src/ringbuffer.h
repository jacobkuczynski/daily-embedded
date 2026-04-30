#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>

/* Capacity must be a power of two so the index wrap is a single AND.
 * One slot is reserved to disambiguate full from empty, so the queue
 * holds at most RB_CAPACITY - 1 bytes. */
#define RB_CAPACITY 16U
#define RB_MASK     (RB_CAPACITY - 1U)

typedef struct {
    /* Producer (ISR) writes head, consumer reads head.
     * Consumer (thread) writes tail, producer reads tail.
     * Both must be volatile so the compiler does not cache them
     * across an ISR boundary. */
    volatile uint8_t head;
    volatile uint8_t tail;
    uint8_t buf[RB_CAPACITY];
} ring_buffer_t;

/**
 * @brief  Initialize an empty ring buffer. Sets head and tail to 0.
 *         Buffer storage is left undefined (you don't need to clear it).
 */
void rb_init(ring_buffer_t *rb);

/**
 * @brief  True iff the buffer holds zero bytes.
 *         Safe to call from the consumer (main-loop) context.
 */
bool rb_is_empty(const ring_buffer_t *rb);

/**
 * @brief  True iff one more rb_push would fail.
 *         Safe to call from the producer (ISR) context.
 */
bool rb_is_full(const ring_buffer_t *rb);

/**
 * @brief  Producer-side push. Called from the ISR.
 *         Writes `byte` into the buffer if there is room.
 *
 *         Contract:
 *           - returns true on success, false if the buffer is full
 *           - does NOT modify tail (consumer's variable)
 *           - the data write to buf[] must happen BEFORE the head
 *             advance, so that a consumer interrupted between the
 *             two never observes a "new" head pointing at stale data.
 */
bool rb_push(ring_buffer_t *rb, uint8_t byte);

/**
 * @brief  Consumer-side pop. Called from the main loop / a thread.
 *         Pops one byte into *out.
 *
 *         Contract:
 *           - returns true on success, false if the buffer is empty
 *           - on false, *out is left untouched
 *           - does NOT modify head (producer's variable)
 *           - the data read from buf[] must happen BEFORE the tail
 *             advance, so that a producer interrupted between the
 *             two never overwrites a slot the consumer is still reading.
 */
bool rb_pop(ring_buffer_t *rb, uint8_t *out);

#endif /* RINGBUFFER_H */
