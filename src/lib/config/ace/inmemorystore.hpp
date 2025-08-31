#pragma once

#include <stdint.h>
#include "configstore.hpp"

/**
 * In memory store that allows to keep it's configuration even between reboots as long as power is on the PICO
 */
template <std::size_t DATA_SIZE>
class InMemoryStore : public ConfigStore
{
    uint16_t position;
    uint8_t *store;

public:
    InMemoryStore(uint8_t *store_) : position(0), store(store_) {};

    void rewind()
    {
        position = 0;
    }

    size_t write(uint8_t c)
    {
        if (position >= DATA_SIZE)
        {
            return 0;
        }

        store[position] = c;
        position += 1;
        return 1;
    }

    size_t write(const uint8_t *data, size_t size)
    {
        if ((size + position) >= DATA_SIZE)
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
