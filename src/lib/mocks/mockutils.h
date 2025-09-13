#pragma once

#include <stdint.h>
#include <stdio.h>

#include "etl/vector.h"
#include "etl/span.h"


template <typename T, typename U>
bool span_equal(const T& a, const U& b) {
    return etl::equal(etl::span<const uint8_t>(a),
                      etl::span<const uint8_t>(b));
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

inline void print_buffer_hex(uint8_t *buffer, uint8_t length)
{
    printf("Length(%d) ", length);
    for (uint8_t i = 0; i < length; i++)
    {
        printf("0x%02X", buffer[i]);
        if (i < length-1)
        {
            printf(", ");
        }
    }
}

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

template<typename T>
inline void print_buffer_hex(const etl::ivector<T>& data)
{
    printf("Length(%d) ", data.size());
    for (auto d : data)
    {
        printf("0x%02X", d);
    }
}