#pragma once
#include "../fanet/fanet.hpp"
#include "../fanet/protocol.hpp"

#include "etl/vector.h"
#include "etl/bit_stream.h"

#include <stdint.h>

using namespace FANET;

void dumpHex(const etl::ivector<uint8_t> &buffer) {
    for (uint8_t byte : buffer) {
        printf("0x%02X, ", byte); // Print each byte in 2-digit uppercase hex
    }
    printf("\n");
}

void dumpHex(const etl::array<uint8_t, 16> &buffer) {
    for (uint8_t byte : buffer) {
        printf("0x%02X, ", byte); // Print each byte in 2-digit uppercase hex
    }
    printf("\n");
}

void dumpHex(const etl::span<uint8_t> &buffer) {
    for (uint8_t byte : buffer) {
        printf("0x%02X, ", byte); // Print each byte in 2-digit uppercase hex
    }
    printf("\n");
}

template <size_t N>
etl::vector<uint8_t, N> makeVector(const uint8_t (&arr)[N]) {
    return etl::vector<uint8_t, N>(std::begin(arr), std::end(arr));
}


template <size_t N>
etl::vector<uint8_t, N> makeVectorS(const etl::span<uint8_t> &span) {
    // Ensure the span size does not exceed the vector's capacity
    if (span.size() > N) {
        // Handle the error case (e.g., throw an exception, return an empty vector, or truncate)
        // For simplicity, this example returns an empty vector
        return etl::vector<uint8_t, N>();
    }
    
    // Construct and return the vector from the span's range
    return etl::vector<uint8_t, N>(span.begin(), span.end());
}

template <size_t SIZE>
etl::vector<uint8_t, SIZE> makeVector(uint8_t value) {
    return etl::vector<uint8_t, SIZE>(SIZE, value);
}

template <typename T>
RadioPacket createRadioPacket(const T& payload) {
    RadioPacket buffer;
    buffer.uninitialized_resize(buffer.capacity());

    etl::bit_stream_writer writer(buffer.data(), buffer.capacity(), etl::endian::big);
    payload.serialize(writer);

    buffer.resize(writer.size_bytes());
    return buffer;
}

etl::bit_stream_reader createReader(const etl::ivector<uint8_t> &buffer) {
    return etl::bit_stream_reader((uint8_t*)buffer.data(), buffer.size(), etl::endian::big);
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



int16_t climbRate_Origional(float climbRate)
{
    int climb10 = constrain((int)std::round(climbRate * 10.0f), -315, 315);
    if (std::abs(climb10) > 63)
        return ((climb10 + (climb10 >= 0 ? 2 : -2)) / 5);
    else
        return climb10;
}


int16_t turnRate_Origional(float turnRate)
{
    int trOs = constrain((int)std::round(turnRate * 4.0f), -254, 254);
    if (std::abs(trOs) >= 63)
        return ((trOs + (trOs >= 0 ? 2 : -2)) / 4);
    else
        return trOs;
}

int16_t speed_Origional(float speed)
{
    int speed2 = constrain((int)std::round(speed * 2.0f), 0, 635);
    if (speed2 > 127)    
        return ((speed2 + 2) / 5);
    else
        return speed2;
}

int16_t altitude_Origional(float altitude)
{
    int alt = constrain(altitude, 0.f, 8190.f);
    if(alt > 2047)
        return (alt+2)/4;
    else
        return alt;    
}


auto OWN_ADDRESS = Address{0x11,0x1111};
auto OTHER_ADDRESS_55 = Address{0x55,0x5555};
auto OTHER_ADDRESS_66 = Address{0x66,0x6666};
auto OTHER_ADDRESS_UNR = Address{0xEE,0xEEEE};
auto BROADCAST_ADDRESS = Address{};
auto IGNORING_ADDRESS = Address{0xff, 0xffff};

const FANET::TxFrame * findByAddress(Protocol &protocol, Header::MessageType type, Address destination, Address source) {
    auto it = etl::find_if(protocol.pool().begin(), protocol.pool().end(),
    [&type, &destination, &source](auto block)
    {
        if (block.type() != type)
        {
            return false;
        }

        if (block.destination() != destination && destination != IGNORING_ADDRESS)
        {
            return false;
        }

        if (block.source() != source && source != IGNORING_ADDRESS)
        {
            return false;
        }

        return true;
    });

    if (it != protocol.pool().end())
    {
        return &(*it);
    }

    return nullptr;
}

 FANET::TxFrame * findByAddress(Protocol &protocol, Address destination, Address source = IGNORING_ADDRESS) {
    auto * it = etl::find_if(protocol.pool().begin(), protocol.pool().end(),
    [&destination, &source](auto block)
    {
        if (block.destination() != destination && destination != IGNORING_ADDRESS)
        {
            return false;
        }

        if (block.source() != source && source != IGNORING_ADDRESS)
        {
            return false;
        }

        return true;
    });

    if (it != protocol.pool().end())
    {
        return  const_cast<FANET::TxFrame *>(&(*it));
    }

    return nullptr;
}