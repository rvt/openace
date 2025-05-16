#pragma once

#include <stdint.h>
#include "etl/array.h"

template <size_t BUFFER_SIZE>
class CircularBuffer
{
    etl::array<char, BUFFER_SIZE> buffer; // Internal buffer
    size_t head = 0;                      // Write position
    size_t tail = 0;                      // Read position
    size_t count = 0;                     // Number of bytes in the buffer

public:
    // 1️⃣ Returns the number of bytes available to write
    size_t available() const
    {
        return BUFFER_SIZE - count;
    }

    size_t length() const
    {
        return count;
    }

    bool empty() const
    {
        return count == 0;
    }

    void clear()
    {
        head = 0;
        tail = 0;
        count = 0;
    }

    bool push(const char *data, size_t len)
    {
        if (len > available())
        {
            return false;
        }

        size_t end_space = BUFFER_SIZE - head;
        if (len <= end_space)
        {
            memcpy(buffer.data() + head, data, len);
        }
        else
        {
            memcpy(buffer.data() + head, data, end_space);
            memcpy(buffer.data(), data + end_space, len - end_space);
        }

        head = (head + len) % BUFFER_SIZE;
        count += len;
        return true;
    }

    // 3️⃣ Peeks up to `maxlen` bytes from the buffer into `dest`
    // Untested!!
    // size_t peek(char* dest, size_t maxlen) const {
    //     size_t to_read = (maxlen < count) ? maxlen : count;

    //     size_t end_space = BufferSize - tail;
    //     if (to_read <= end_space) {
    //         // Data is contiguous
    //         memcpy(dest, &buffer[tail], to_read);
    //     } else {
    //         // Split the data into two parts
    //         memcpy(dest, &buffer[tail], end_space);
    //         memcpy(dest + end_space, buffer, to_read - end_space);
    //     }

    //     return to_read;
    // }

    // Returns a direct pointer to the internal buffer and the length of contiguous data
    auto peek() const
    {
        struct PeekResult
        {
            const char *part;
            size_t size;
        };

        if (count == 0)
        {
            return PeekResult{nullptr, 0};
        }

        return PeekResult{buffer.data() + tail, std::min(count, BUFFER_SIZE - tail)};
    }

    // Similar to peek, but will directly accept the data
    auto get()
    {
        struct PeekResult
        {
            const char *part;
            size_t size;
        };

        if (count == 0)
        {
            return PeekResult{nullptr, 0};
        }

        auto size = std::min(count, BUFFER_SIZE - tail);
        auto mTail = tail;
        accepted(size);
        return PeekResult{buffer.data() + mTail, size};
    }

    // 4️⃣ Advances the "read pointer" by `len` bytes
    void accepted(size_t len)
    {
        if (len > count)
        {
            len = count; // Prevent overflow
        }
        tail = (tail + len) % BUFFER_SIZE;
        count -= len;
    }

    /**
     * Compact the buffer. At this moment it does not yet support wrapped buffer and will return false
     * It will move all remaining data to the beginning of the buffer
     */
    bool compact()
    {
        if (count == 0 || tail == 0)
        {
            // Nothing to do — already compact or empty
            return true;
        }

        if (tail < head)
        {
            // Data is linear, safe to shift
            memmove(buffer.data(), buffer.data() + tail, count);
            tail = 0;
            head = count;
            return true;
        }

        // Buffer is wrapped — not supported without a temporary buffer
        return false;
    }

    // Debug utility to print internal buffer state
    void debug() const
    {
        printf("Buffer State: head=%zu, tail=%zu, count=%zu\n", head, tail, count);
        printf("Buffer: ");
        for (size_t i = 0; i < BUFFER_SIZE; ++i)
        {
            if (i == head)
                printf("[H]");
            if (i == tail)
                printf("[T]");
            printf("%c", (unsigned char)buffer[i]);
        }
        printf("\n");
    }
};
