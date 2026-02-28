#pragma once

#include <stdint.h>
#include <stdio.h>

#include "etl/vector.h"
#include "etl/span.h"
#include <inttypes.h>

template <typename T, typename U>
inline bool span_equal(const T& a, const U& b) {
    return etl::equal(etl::span<const uint8_t>(a),
                      etl::span<const uint8_t>(b));
}

template <typename T>
inline void printBufferHex(etl::span<T> buffer)
{
    static_assert(std::is_integral<T>::value, "T must be an integral type");

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

inline void print_binary(unsigned int number)
{
    if (number >> 1)
    {
        print_binary(number >> 1);
    }
    putc((number & 1) ? '1' : '0', stdout);
}

template<typename T>
inline void print_buffer_dec(T *buffer, uint8_t length)
{
    printf("Length(%d) ", length);
    for (uint8_t i = 0; i < length; i++)
    {
        printf("%d", buffer[i]);
        if (i < length-1)
        {
            printf(", ");
        }
    }
}

// Deprecated
inline void print_buffer_hex(uint8_t *buffer, uint8_t length)
{
    printBufferHex(etl::span(buffer, length));
}

// Deprecated
inline void print_buffer_hex_uint32(uint32_t *buffer, uint8_t length)
{
    printf("Length(%d) ", length);
    for (uint8_t i = 0; i < length; i++)
    {
        printf("0x%08X", buffer[i]);
        if (i < length-1)
        {
            printf(", ");
        }
    }
}

// Deprecated
template<typename T>
inline void print_buffer_hex(const etl::ivector<T>& data)
{
    printf("Length(%d) ", data.size());
    for (auto d : data)
    {
        printf("0x%02X", d);
    }
}

