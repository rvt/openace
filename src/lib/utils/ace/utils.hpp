#pragma once

#include "hardware/pio.h"
#include "stdint.h"
#include "stddef.h"
#include "bitcount.hpp"
#include "etl/memory.h"

// Find a free pio and state machine and load the program into it.
// Returns false if this fails
bool add_pio_program(const pio_program_t *program, PIO *pio_hw, int *sm, uint *offset);

/**
 * Shift left a XX number of bits in the Data array
 */
template <bool strict = false>
void bitShift(uint8_t Data[], uint8_t Bytes, uint8_t Shift)
{
  if (Shift == 0)
  {
    return;
  }

  uint16_t totalBits = (uint16_t)Bytes * 8;
  if (Shift >= totalBits)
  {
    if (strict)
    {
      etl::memory_set(static_cast<volatile char *>(static_cast<void *>(Data)), Bytes, 0);
    }
    return;
  }

  uint8_t ByteOfs = Shift >> 3;
  uint8_t BitShift = Shift & 7;
  uint8_t remaining = Bytes - ByteOfs;

  if (BitShift == 0)
  {
    etl::mem_move(Data + ByteOfs, Data + ByteOfs + remaining, Data);
    if (strict)
    {
      etl::memory_set(static_cast<volatile char *>(static_cast<void *>(Data + remaining)), Bytes - remaining, 0);
    }
    return;
  }

  uint8_t CmplShift = 8 - BitShift;
  for (uint8_t i = 0; i < remaining; i++)
  {
    uint8_t hi = Data[i + ByteOfs] << BitShift;
    uint8_t lo = 0;
    if (i + ByteOfs + 1 < Bytes)
    {
      lo = Data[i + ByteOfs + 1] >> CmplShift;
    }
    Data[i] = hi | lo;
  }
  if (strict)
  {
    etl::memory_set(static_cast<volatile char *>(static_cast<void *>(Data + remaining)), Bytes - remaining, 0);
  }
  return;
}


template <size_t WordCount>
uint8_t diffBits(const uint32_t Data[],
                 const uint32_t Ref[],
                 const uint32_t Mask[])
{
    uint8_t Count = 0;

    for (uint32_t i = 0; i < WordCount; ++i)
    {
        uint32_t x = (Data[i] ^ Ref[i]) & Mask[i];
        Count += Count1s(x);
    }

    return Count;
}

template <size_t WordCount>
uint8_t diffBits(const uint32_t Data[], const uint32_t Ref[])
{
    uint8_t Count = 0;

    for (uint32_t Idx = 0; Idx < WordCount; Idx++)
    {
        uint32_t Xor = Data[Idx] ^ Ref[Idx];
        Count += Count1s(Xor);
    }

    return Count;
}

