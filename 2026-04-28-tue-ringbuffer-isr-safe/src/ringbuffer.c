#include "ringbuffer.h"

void rb_init(ring_buffer_t *rb) {
    /* TODO: zero head and tail. You don't need to clear buf[]. */
    (void)rb;
}

bool rb_is_empty(const ring_buffer_t *rb) {
    /* TODO: return true iff head == tail. */
    (void)rb;
    return false;
}

bool rb_is_full(const ring_buffer_t *rb) {
    /* TODO: return true iff (head + 1) wrapped == tail. */
    (void)rb;
    return false;
}

bool rb_push(ring_buffer_t *rb, uint8_t byte) {
    /* TODO: producer side.
     *  1. if full, return false (do NOT touch tail).
     *  2. write `byte` into buf[head].
     *  3. THEN advance head with the AND-mask wrap.
     *  4. return true.
     * Order of (2) and (3) matters — see the header comment. */
    (void)rb;
    (void)byte;
    return false;
}

bool rb_pop(ring_buffer_t *rb, uint8_t *out) {
    /* TODO: consumer side.
     *  1. if empty, return false. Do NOT write *out, do NOT touch head.
     *  2. read buf[tail] into *out.
     *  3. THEN advance tail with the AND-mask wrap.
     *  4. return true. */
    (void)rb;
    (void)out;
    return false;
}
