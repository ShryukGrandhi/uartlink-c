/* Host-side unit tests. Build and run: make test */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ringbuf.h"
#include "crc16.h"
#include "frame.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void test_crc(void) {
    const uint8_t s[] = "123456789";
    CHECK(crc16(s, 9) == 0x29B1);                      /* CRC-16/CCITT-FALSE check value */
    CHECK(crc16(s, 0) == 0xFFFF);
}

static void test_ringbuf(void) {
    uint8_t store[8], out[16];
    ringbuf_t rb;
    CHECK(!ringbuf_init(&rb, store, 6));               /* not a power of two */
    CHECK(ringbuf_init(&rb, store, 8));
    CHECK(ringbuf_count(&rb) == 0 && ringbuf_free(&rb) == 8);
    uint8_t b;
    CHECK(!ringbuf_get(&rb, &b));                      /* empty */
    const uint8_t data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    CHECK(ringbuf_write(&rb, data, 10) == 8);          /* only capacity fits */
    CHECK(!ringbuf_put(&rb, 42));                      /* full */
    CHECK(ringbuf_read(&rb, out, 3) == 3 && out[0] == 0 && out[2] == 2);
    CHECK(ringbuf_write(&rb, data, 3) == 3);           /* wraps around the end */
    CHECK(ringbuf_count(&rb) == 8);
    CHECK(ringbuf_read(&rb, out, 16) == 8);
    CHECK(out[0] == 3 && out[4] == 7 && out[5] == 0 && out[7] == 2);
    /* many laps: indices wrap without losing order */
    for (int lap = 0; lap < 1000; lap++) {
        CHECK(ringbuf_put(&rb, (uint8_t)lap));
        CHECK(ringbuf_get(&rb, &b) && b == (uint8_t)lap);
    }
}

static size_t decode_all(frame_decoder_t *d, const uint8_t *s, size_t n, uint8_t got[][64], size_t *lens, size_t max) {
    size_t k = 0, len;
    for (size_t i = 0; i < n; i++)
        if (frame_decoder_feed(d, s[i], &len) == FRAME_OK && k < max) {
            memcpy(got[k], d->buf, len);
            lens[k++] = len;
        }
    return k;
}

static void test_frame_roundtrip_and_stuffing(void) {
    const uint8_t payload[] = {0x01, FRAME_FLAG, 0x02, FRAME_ESC, 0x03};
    uint8_t wire[64], store[64], got[4][64];
    size_t lens[4];
    size_t n = frame_encode(payload, sizeof payload, wire, sizeof wire);
    CHECK(n > sizeof payload + 4);
    for (size_t i = 1; i + 1 < n; i++) CHECK(wire[i] != FRAME_FLAG);   /* no flag inside a frame */
    frame_decoder_t d;
    frame_decoder_init(&d, store, sizeof store);
    CHECK(decode_all(&d, wire, n, got, lens, 4) == 1);
    CHECK(lens[0] == sizeof payload && memcmp(got[0], payload, sizeof payload) == 0);
    CHECK(frame_encode(payload, sizeof payload, wire, 4) == 0);         /* output too small */
}

static void test_frame_noise_and_corruption(void) {
    const uint8_t a[] = "hello", b[] = "world";
    uint8_t wire[128], store[64], got[4][64];
    size_t lens[4], n = 0;
    const uint8_t noise[] = {0x55, 0xAA, 0x13};
    memcpy(wire, noise, sizeof noise); n += sizeof noise;               /* garbage before the first flag */
    n += frame_encode(a, 5, wire + n, sizeof wire - n);
    size_t corrupt_at = n + 2;
    n += frame_encode(b, 5, wire + n, sizeof wire - n);
    wire[corrupt_at] ^= 0x01;                                           /* flip a bit in frame 2 */
    n += frame_encode(a, 5, wire + n, sizeof wire - n);
    frame_decoder_t d;
    frame_decoder_init(&d, store, sizeof store);
    size_t k = decode_all(&d, wire, n, got, lens, 4);
    CHECK(k == 2);                                                      /* the corrupted frame is dropped */
    CHECK(lens[0] == 5 && memcmp(got[0], "hello", 5) == 0);
    CHECK(lens[1] == 5 && memcmp(got[1], "hello", 5) == 0);
    CHECK(d.bad_crc == 1 && d.ok == 2);
}

static void test_frame_overflow(void) {
    uint8_t big[40], wire[128], store[16], got[2][64];
    size_t lens[2];
    memset(big, 0x11, sizeof big);
    size_t n = frame_encode(big, sizeof big, wire, sizeof wire);
    n += frame_encode((const uint8_t *)"ok", 2, wire + n, sizeof wire - n);
    frame_decoder_t d;
    frame_decoder_init(&d, store, sizeof store);                        /* smaller than the first frame */
    CHECK(decode_all(&d, wire, n, got, lens, 2) == 1);
    CHECK(lens[0] == 2 && memcmp(got[0], "ok", 2) == 0 && d.overflow == 1);
}

static void test_loopback_through_ringbuf(void) {
    /* Frames go through a small ring buffer in chunks, as a UART RX interrupt would deliver them. */
    uint8_t rstore[32], fstore[64], wire[2048], got[1][64];
    ringbuf_t rb;
    frame_decoder_t d;
    ringbuf_init(&rb, rstore, sizeof rstore);
    frame_decoder_init(&d, fstore, sizeof fstore);
    size_t n = 0, sent = 0;
    srand(7);
    for (int f = 0; f < 50; f++) {
        uint8_t p[24];
        for (int i = 0; i < 24; i++) p[i] = (uint8_t)rand();
        n += frame_encode(p, 1 + (size_t)(rand() % 24), wire + n, sizeof wire - n);
        sent++;
    }
    size_t pos = 0, ok = 0, len;
    while (pos < n || ringbuf_count(&rb)) {
        pos += ringbuf_write(&rb, wire + pos, (size_t)(rand() % 9));   /* irregular arrival */
        uint8_t b;
        while (ringbuf_get(&rb, &b))
            if (frame_decoder_feed(&d, b, &len) == FRAME_OK) { memcpy(got[0], d.buf, len); ok++; }
    }
    CHECK(ok == sent && d.bad_crc == 0);
}

int main(void) {
    test_crc();
    test_ringbuf();
    test_frame_roundtrip_and_stuffing();
    test_frame_noise_and_corruption();
    test_frame_overflow();
    test_loopback_through_ringbuf();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
