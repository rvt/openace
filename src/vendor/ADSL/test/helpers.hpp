#pragma once

#include "../include/adsl/header.hpp"
#include "../include/adsl/networkpayload.hpp"
#include "../include/adsl/utils.hpp"
#include "../include/adsl/framebuffer.hpp"
#include "etl/bit_stream.h"
#include "etl/vector.h"

#include <stdint.h>
#include <inttypes.h>

using namespace ADSL;

using TestRadioPacket = etl::vector<uint8_t, 255>;

template <typename T>
inline void printBufferHex(etl::span<T> buffer)
{

  printf("Length(%d) ", static_cast<int>(buffer.size()));

  for (size_t i = 0; i < buffer.size(); ++i)
  {
    if constexpr (sizeof(T) == 1)
      printf("0x%02" PRIX8, static_cast<uint8_t>(buffer[i]));
    else if constexpr (sizeof(T) == 2)
      printf("0x%04" PRIX16, static_cast<uint16_t>(buffer[i]));
    else if constexpr (sizeof(T) == 4)
      printf("0x%08" PRIX32, static_cast<uint32_t>(buffer[i]));
    else
      printf("0x%X", static_cast<unsigned int>(buffer[i])); // fallback

    if (i + 1 < buffer.size())
      printf(", ");
  }
}

template <size_t N>
etl::vector<uint8_t, N> makeVector(const uint8_t (&arr)[N])
{
  return etl::vector<uint8_t, N>(std::begin(arr), std::end(arr));
}

template <size_t N>
etl::vector<uint8_t, N> makeVectorS(const etl::span<uint8_t> &span)
{
  if (span.size() > N)
  {
    return etl::vector<uint8_t, N>();
  }

  return etl::vector<uint8_t, N>(span.begin(), span.end());
}

template <size_t SIZE>
etl::vector<uint8_t, SIZE> makeVector(uint8_t value)
{
  return etl::vector<uint8_t, SIZE>(SIZE, value);
}

template <typename T>
TestRadioPacket createRadioPacket(const T &payload)
{
  TestRadioPacket buffer;
  buffer.uninitialized_resize(buffer.capacity());

  etl::bit_stream_writer writer(buffer.data(), buffer.capacity(), etl::endian::little);
  payload.serialize_issue1(writer);

  buffer.resize(writer.size_bytes());
  return buffer;
}

// Overload that accepts a serializer callable (e.g. a lambda) so tests
// can perform custom serialization outside of concrete payload types.
template <typename F>
TestRadioPacket createRadioPacket(F &&serializer)
{
  TestRadioPacket buffer;
  buffer.uninitialized_resize(buffer.capacity());

  etl::bit_stream_writer writer(buffer.data(), buffer.capacity(), etl::endian::little);
  serializer(writer);

  buffer.resize(writer.size_bytes());
  return buffer;
}

// Build a complete radio packet (network header + ADSL header + payload + CRC)
// - `header`: the ADSL header to serialize
// - `networkPayload`: the network-layer byte to insert (protocol version, flags, ...)
// - `serializer`: callable that accepts an `etl::bit_stream_writer &` and writes the payload bits
template <size_t FRAME_WORD_SIZE, typename F>
FrameBuffer<FRAME_WORD_SIZE> buildRadioPacket(const Header &header, const NetworkPayload &networkPayload, F &&serializer)
{
  FrameBuffer<FRAME_WORD_SIZE> frame;
  etl::bit_stream_writer writer(frame.fullSpan(), etl::endian::little);

  // Serialize Header
  frame.used_bytes = writer.size_bytes();
  header.serialize(writer);
  frame.used_bytes = writer.size_bytes();

  // Serialize Payload
  serializer(writer);
  frame.used_bytes = writer.size_bytes();

  // Flip bytes to match memory layout
  frame.reverseBits();

  // Encrypt header + payload that is now byte aligned
  if (networkPayload.keyIndex() < 3)
  {
    XXTEA_Encrypt_Key0(frame.words(), frame.storage.size() - 1, 6);
  }

  // Add the network header at the beginning, not part of encryption
  // it's not part of scramble, but part of the CRC but scramble needs to be 32bit aligned
  frame.used_bytes = writer.size_bytes();
  writer.write_unchecked(0x88, 8);

  auto bspan = frame.fullSpan();
  etl::mem_move(&bspan[0], frame.maxSizeBytes() - 1, &bspan[1]);
  frame.used_bytes = writer.size_bytes();
  frame[0] = etl::reverse_bits(networkPayload.asUint8());

  // Finally calculate the CRC
  uint32_t crc = ADSL::calcPI(frame.usedSpan());
  writer.write_unchecked(etl::reverse_bits<uint32_t>(crc) >> 8, 24);
  frame.used_bytes = writer.size_bytes();
  return frame;
}

etl::bit_stream_reader createReader(const etl::ivector<uint8_t> &buffer)
{
  return etl::bit_stream_reader((uint8_t *)buffer.data(), buffer.size(), etl::endian::little);
}

template <class T>
const T &constrain(const T &x, const T &lo, const T &hi)
{
  if (x < lo)
    return lo;
  else if (hi < x)
    return hi;
  else
    return x;
}
