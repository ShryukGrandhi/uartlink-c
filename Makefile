CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude
SAN = -fsanitize=address,undefined -fno-omit-frame-pointer
SRC = src/ringbuf.c src/crc16.c src/frame.c

.PHONY: test clean
test: build/tests
	./build/tests

build/tests: $(SRC) tests/test_main.c include/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SAN) -g $(SRC) tests/test_main.c -o $@

clean:
	rm -rf build
