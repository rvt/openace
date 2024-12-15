#include <stdint.h>

template <size_t BufferSize>
class CircularBuffer
{
    char buffer[BufferSize]; // Internal buffer
    size_t head = 0;         // Write position
    size_t tail = 0;         // Read position
    size_t count = 0;        // Number of bytes in the buffer

public:
    // 1️⃣ Returns the number of bytes available to write
    size_t available() const
    {
        return BufferSize - count;
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

    // 2️⃣ Pushes `len` bytes from `data` into the buffer
    bool push(const char *data, size_t len)
    {
        if (len > available())
        {
            return false; // Not enough space
        }

        size_t end_space = BufferSize - head;
        if (len <= end_space)
        {
            // Data fits in the remaining space at the end
            memcpy(&buffer[head], data, len);
            head = (head + len) % BufferSize;
        }
        else
        {
            // Split the data into two parts
            memcpy(&buffer[head], data, end_space);
            memcpy(buffer, data + end_space, len - end_space);
            head = (len - end_space);
        }

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

    // 3️⃣ Returns a direct pointer to the internal buffer and the length of contiguous data
    void peek(const char *&part, size_t &len) const
    {
        if (count == 0)
        {
            part = nullptr;
            len = 0;
            return;
        }

        part = &buffer[tail];
        len = std::min(count, BufferSize - tail); // Return contiguous region before wraparound
    }

    // 4️⃣ Advances the "read pointer" by `len` bytes
    void accepted(size_t len)
    {
        if (len > count)
        {
            len = count; // Prevent overflow
        }
        tail = (tail + len) % BufferSize;
        count -= len;
    }

    // Debug utility to print internal buffer state
    void debug() const
    {
        printf("Buffer State: head=%zu, tail=%zu, count=%zu\n", head, tail, count);
        printf("Buffer: ");
        for (size_t i = 0; i < BufferSize; ++i)
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
