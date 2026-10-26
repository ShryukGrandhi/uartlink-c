#include "frame.h"
#include "crc16.h"

static size_t put_stuffed(uint8_t b, uint8_t *out, size_t pos, size_t cap) {
    if (b == FRAME_FLAG || b == FRAME_ESC) {
        if (pos + 2 > cap) return 0;
        out[pos] = FRAME_ESC;
        out[pos + 1] = (uint8_t)(b ^ FRAME_XOR);
        return pos + 2;
    }
    if (pos + 1 > cap) return 0;
    out[pos] = b;
    return pos + 1;
}

size_t frame_encode(const uint8_t *payload, size_t n, uint8_t *out, size_t out_cap) {
    size_t pos = 0;
    if (out_cap < 1) return 0;
    out[pos++] = FRAME_FLAG;
    uint16_t crc = crc16(payload, n);
    for (size_t i = 0; i < n; i++)
        if ((pos = put_stuffed(payload[i], out, pos, out_cap)) == 0) return 0;
    if ((pos = put_stuffed((uint8_t)(crc >> 8), out, pos, out_cap)) == 0) return 0;
    if ((pos = put_stuffed((uint8_t)(crc & 0xFF), out, pos, out_cap)) == 0) return 0;
    if (pos + 1 > out_cap) return 0;
    out[pos++] = FRAME_FLAG;
    return pos;
}

void frame_decoder_init(frame_decoder_t *d, uint8_t *storage, size_t cap) {
    d->buf = storage;
    d->cap = cap;
    d->len = 0;
    d->in_frame = d->escaped = 0;
    d->ok = d->bad_crc = d->overflow = 0;
}

frame_status_t frame_decoder_feed(frame_decoder_t *d, uint8_t byte, size_t *out_len) {
    if (byte == FRAME_FLAG) {
        frame_status_t st = FRAME_NONE;
        if (d->in_frame && d->len >= 2 && !d->escaped) {
            /* last two bytes are the CRC of everything before them */
            uint16_t want = (uint16_t)((d->buf[d->len - 2] << 8) | d->buf[d->len - 1]);
            if (crc16(d->buf, d->len - 2) == want) {
                *out_len = d->len - 2;
                d->ok++;
                st = FRAME_OK;
            } else {
                d->bad_crc++;
                st = FRAME_BAD_CRC;
            }
        }
        /* a flag both ends one frame and may start the next */
        d->in_frame = 1;
        d->len = 0;
        d->escaped = 0;
        return st;
    }
    if (!d->in_frame) return FRAME_NONE;              /* noise before the first flag */
    if (byte == FRAME_ESC) {
        d->escaped = 1;
        return FRAME_NONE;
    }
    if (d->escaped) {
        byte ^= FRAME_XOR;
        d->escaped = 0;
    }
    if (d->len >= d->cap) {                            /* too long: drop until the next flag */
        d->in_frame = 0;
        d->len = 0;
        d->overflow++;
        return FRAME_OVERFLOW;
    }
    d->buf[d->len++] = byte;
    return FRAME_NONE;
}
