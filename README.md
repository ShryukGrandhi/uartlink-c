# uartlink-c

A small C99 library for moving framed messages over a byte stream such as a UART, written the way
firmware usually has to be: no heap allocation, fixed buffers owned by the caller, and a decoder
that processes one byte at a time.

It has three parts:

- `ringbuf` — a fixed-capacity single-producer / single-consumer byte ring buffer. One side (for
  example a UART receive interrupt) only writes `head`, the other (the main loop) only writes
  `tail`, so on a single core no lock is needed. Capacity must be a power of two.
- `crc16` — CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF).
- `frame` — HDLC-style framing: `0x7E | payload + CRC | 0x7E`, with `0x7E`/`0x7D` inside a frame
  escaped as `0x7D, byte ^ 0x20`. The decoder is a state machine that ignores noise before the
  first flag, drops frames with a bad CRC or that exceed its buffer, and resynchronises on the next
  flag. It keeps counters (`ok`, `bad_crc`, `overflow`) for diagnostics.

## Run the tests

```sh
make test            # builds with -Wall -Wextra -Werror and AddressSanitizer/UBSan, then runs
make test CC=clang
```

The tests cover the CRC check value, ring-buffer wraparound, byte stuffing, noise before a frame,
a corrupted frame between two good ones, an oversized frame, and a loopback where 50 random frames
arrive through a 32-byte ring buffer in irregular chunks. CI runs them with gcc and clang on every push.

## Use

```c
uint8_t rx_store[64], frame_store[128];
ringbuf_t rx;  frame_decoder_t dec;
ringbuf_init(&rx, rx_store, sizeof rx_store);
frame_decoder_init(&dec, frame_store, sizeof frame_store);

/* in the UART RX interrupt */      ringbuf_put(&rx, UART->DR);
/* in the main loop */
uint8_t b; size_t len;
while (ringbuf_get(&rx, &b))
    if (frame_decoder_feed(&dec, b, &len) == FRAME_OK) handle(dec.buf, len);
```

This is a host-tested library; it has not been run on a specific microcontroller.
