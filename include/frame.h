/* HDLC-style framing for a byte stream such as a UART:
 *   0x7E | stuffed(payload | crc16_hi | crc16_lo) | 0x7E
 * Bytes 0x7E and 0x7D inside a frame are sent as 0x7D, byte ^ 0x20.
 * The decoder is a byte-at-a-time state machine: it resynchronises on the next flag after
 * noise, and drops frames that are too long or whose CRC does not match. */
#ifndef FRAME_H
#define FRAME_H
#include <stddef.h>
#include <stdint.h>

#define FRAME_FLAG 0x7E
#define FRAME_ESC  0x7D
#define FRAME_XOR  0x20

/* Encode payload into out (worst case 2*n + 6 bytes). Returns bytes written, 0 if out is too small. */
size_t frame_encode(const uint8_t *payload, size_t n, uint8_t *out, size_t out_cap);

typedef enum { FRAME_NONE = 0, FRAME_OK, FRAME_BAD_CRC, FRAME_OVERFLOW } frame_status_t;

typedef struct {
    uint8_t *buf;
    size_t cap, len;
    int in_frame, escaped;
    unsigned long ok, bad_crc, overflow;   /* counters for diagnostics */
} frame_decoder_t;

void frame_decoder_init(frame_decoder_t *d, uint8_t *storage, size_t cap);
/* Feed one byte. On FRAME_OK the payload is d->buf[0..*out_len). */
frame_status_t frame_decoder_feed(frame_decoder_t *d, uint8_t byte, size_t *out_len);
#endif
