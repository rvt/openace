#pragma once

#include <stdint.h>
#include "etl/span.h"

/**

* @class DelimiterBitmap
* @brief Efficient compile-time and runtime bitmap for delimiter lookup in byte streams.
*
* This class provides a fast way to check whether a given byte is one of a predefined set
* of delimiters (e.g., CR/LF or null-terminated data). Internally it stores a 256-bit
* bitmap (32 bytes) where each bit corresponds to a possible byte value.
*
* Typical use case:
* * Stream parsers that need to detect end-of-line markers like '\r' or '\n'.
* * Frame or message boundary detection based on delimiter bytes.
*
* ### Example
* ```cpp
  ```
* auto& delimiters = DelimiterBitmap::CRLF();
* if (delimiters.contains(currentByte)) {
* ```
  // End of line or message
  ```
* }
* ```
  ```
*
* The class can be instantiated with a custom set of delimiters using an `etl::span`
* or you can use predefined static instances (`CRLF()`, `Null()`).
*
* This implementation is constexpr-capable and can be used in compile-time contexts.
*/
class DelimiterBitmap
{
private:
    uint8_t bitmap_[256 / 8]{};
    constexpr void add(uint8_t c)
    {
        bitmap_[c >> 3] |= (1u << (c & 7));
    }

    constexpr const uint8_t *data() const { return bitmap_; }

public:
    constexpr DelimiterBitmap() : bitmap_{} {}

    constexpr DelimiterBitmap(etl::span<const uint8_t> delimiters) : bitmap_{}
    {
        for (auto b : delimiters)
        {
            add(b);
        }
    }

    constexpr bool contains(uint8_t c) const
    {
        return (bitmap_[c >> 3] & (1u << (c & 7))) != 0;
    }

    static const DelimiterBitmap& CRLF()
    {
        static constexpr auto delimiters = etl::make_array<uint8_t>('\r', '\n');
        static constexpr DelimiterBitmap instance{etl::span<const uint8_t>{delimiters}};
        return instance;
    }

    static const DelimiterBitmap& Null()
    {
        static constexpr auto delimiters = etl::make_array<uint8_t>(0);
        static constexpr DelimiterBitmap instance{etl::span<const uint8_t>{delimiters}};
        return instance;
    }
};
