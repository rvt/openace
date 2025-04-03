// Fast bit counting for (forward) error correction codes
// (c) 2003, Pawel Jalocha, Pawel.Jalocha@cern.ch
//
// Added with permission of Pawel Jalocha
#pragma once

#include <stdint.h>

// ==========================================================================

inline uint8_t Count1s(uint8_t Byte)
{
    return __builtin_popcount(Byte);
}


inline uint8_t Count1s(uint16_t Word)
{
    return __builtin_popcount(Word);
}


inline uint8_t Count1s(uint32_t LongWord)
{
    return __builtin_popcountl(LongWord);
}

inline uint8_t Count1s(int32_t LongWord)
{
    return Count1s((uint32_t)LongWord);
}

inline uint8_t Count1s(uint64_t LongWord)
{
    return __builtin_popcountll(LongWord);
}

inline uint8_t Count1s(int64_t LongWord)
{
    return Count1s((uint64_t)LongWord);
}

int Count1s(const uint8_t *Byte, int Bytes);

// use __builtin_popcount(unsigned int) ? http://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
