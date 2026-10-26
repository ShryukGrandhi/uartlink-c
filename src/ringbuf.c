#include "ringbuf.h"

bool ringbuf_init(ringbuf_t *rb, uint8_t *storage, size_t capacity) {
    if (!rb || !storage || capacity < 2 || (capacity & (capacity - 1)) != 0) return false;
    rb->buf = storage;
    rb->mask = capacity - 1;
    rb->head = rb->tail = 0;
    return true;
}

size_t ringbuf_count(const ringbuf_t *rb) { return (rb->head - rb->tail) & (rb->mask * 2 + 1); }

/* head and tail run freely over 0..2*capacity-1 so "full" and "empty" are distinguishable. */
size_t ringbuf_free(const ringbuf_t *rb) { return (rb->mask + 1) - ringbuf_count(rb); }

bool ringbuf_put(ringbuf_t *rb, uint8_t byte) {
    if (ringbuf_free(rb) == 0) return false;
    rb->buf[rb->head & rb->mask] = byte;
    rb->head = (rb->head + 1) & (rb->mask * 2 + 1);
    return true;
}

bool ringbuf_get(ringbuf_t *rb, uint8_t *byte) {
    if (ringbuf_count(rb) == 0) return false;
    *byte = rb->buf[rb->tail & rb->mask];
    rb->tail = (rb->tail + 1) & (rb->mask * 2 + 1);
    return true;
}

size_t ringbuf_write(ringbuf_t *rb, const uint8_t *src, size_t n) {
    size_t i = 0;
    while (i < n && ringbuf_put(rb, src[i])) i++;
    return i;
}

size_t ringbuf_read(ringbuf_t *rb, uint8_t *dst, size_t n) {
    size_t i = 0;
    while (i < n && ringbuf_get(rb, &dst[i])) i++;
    return i;
}
