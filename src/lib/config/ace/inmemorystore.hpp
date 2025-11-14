#pragma once

#include <stdint.h>
#include "configstore.hpp"

/**
 * In memory store that allows to keep it's configuration even between reboots as long as power is on the PICO
 */
class InMemoryStore : public ConfigStore
{
    const uint16_t totalSize;
    uint8_t * const store;
    size_t position;
public:
    InMemoryStore(uint16_t totalSize_, uint8_t *store_) : totalSize(totalSize_),  store(store_) , position(0){};

    void rewind()
    {
        position = 0;
    }

    size_t write(uint8_t c)
    {
        if (position >= totalSize)
        {
            return 0;
        }

        store[position] = c;
        position += 1;
        return 1;
    }

    size_t write(const uint8_t *data, size_t size)
    {
        if ((size + position) >= totalSize)
        {
            return 0;
        }
        memcpy(store + position, data, size);
        position += size;
        return size;
    }

    const uint8_t *data() const
    {
        return (const uint8_t *)store;
    }
};
