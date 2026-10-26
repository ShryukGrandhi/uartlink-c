/* Fixed-capacity single-producer / single-consumer byte ring buffer.
 * No heap: the caller owns the storage. Capacity must be a power of two.
 * Safe for one producer (e.g. a UART receive interrupt) and one consumer (the main loop)
 * on a single core: each index is written by exactly one side. */
#ifndef RINGBUF_H
#define RINGBUF_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t *buf;
    size_t mask;               /* capacity - 1 */
    volatile size_t head;      /* written by the producer only */
    volatile size_t tail;      /* written by the consumer only */
} ringbuf_t;

bool   ringbuf_init(ringbuf_t *rb, uint8_t *storage, size_t capacity);
bool   ringbuf_put(ringbuf_t *rb, uint8_t byte);           /* false when full */
bool   ringbuf_get(ringbuf_t *rb, uint8_t *byte);          /* false when empty */
size_t ringbuf_write(ringbuf_t *rb, const uint8_t *src, size_t n);
size_t ringbuf_read(ringbuf_t *rb, uint8_t *dst, size_t n);
size_t ringbuf_count(const ringbuf_t *rb);
size_t ringbuf_free(const ringbuf_t *rb);
#endif
