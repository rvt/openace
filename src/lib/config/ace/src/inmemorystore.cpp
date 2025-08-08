#include <stdint.h>
#include "../inmemorystore.hpp"

#include "pico.h"

constexpr size_t DATA_SIZE = 4096;

// Keep data in RAM between restart
uint8_t __uninitialized_ram(store[DATA_SIZE]);

void InMemoryStore::rewind()
{
    position = 0;
}

size_t InMemoryStore::write(uint8_t c)
{
    if (position >= DATA_SIZE)
    {
        return 0;
    }

    store[position] = c;
    position += 1;
    return 1;
}

size_t InMemoryStore::write(const uint8_t *data, size_t size)
{
    if ((size + position) >= DATA_SIZE)
    {
        return 0;
    }
    memcpy(store + position, data, size);
    position += size;
    return size;
}

const uint8_t *InMemoryStore::data() const
{
    return (const uint8_t*)store;
}
