#pragma once

#include "stdint.h"

void manchesterEncode(uint8_t *destination, const uint8_t* source, uint8_t sourceLength);

/**
 * Manchester Decode a buffer of maximum length 255
*/
void manchesterDecode(uint8_t buffer[], uint8_t bufferLength);
/**
 * Decode a manchester encoded buffer into a destination buffer of half the length of the source
*/
void manchesterDecode(uint8_t destination[], const uint8_t source[], uint8_t sourceLength);

/**
 * Decode a manchester encoded buffer into a destination buffer of half the length of the source
 * This version includes the err frame for galagan correction
*/
void manchesterDecode(uint8_t destination[], uint8_t *err, const uint8_t source[], uint8_t sourceLength);

/**
 * Calculate the parity of a given byte buffer
*/
uint8_t buffersParity8(const uint8_t buffer[], uint16_t bytes);

/**
 * Swap to low order and high order 8 bit
*/
inline uint16_t swapBytes16(uint16_t value)
{
    return (value >> 8) | (value << 8);
}


