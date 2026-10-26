/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final XOR.
 * Check value for "123456789" is 0x29B1. */
#ifndef CRC16_H
#define CRC16_H
#include <stddef.h>
#include <stdint.h>
uint16_t crc16_update(uint16_t crc, uint8_t byte);
uint16_t crc16(const uint8_t *data, size_t n);
#endif
