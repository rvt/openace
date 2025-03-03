#pragma once

#include <stdint.h>
#include <stdio.h>
#include "etl/vector.h"
#include "etl/bitset.h"
#include "etl/span.h"

/**
 * A simple block allocator that allocates blocks from a fixed-size memory pool.
 * The goal is to have a fixed memory pool and allocate memory blocks dynamically,
 * ensuring that each allocated object maintains a reference to its memory.
 */
template <typename T, size_t MAX_BLOCKS, size_t BLOCK_SIZE>
class BlockAllocator
{
private:
    etl::vector<uint8_t, MAX_BLOCKS * BLOCK_SIZE> memoryPool; // Fixed memory pool
    etl::bitset<MAX_BLOCKS> allocationMap;                    // Bit array for tracking used blocks
    etl::vector<T, MAX_BLOCKS> allocatedBlocks;               // Stores allocated objects

public:
    BlockAllocator()
    {
        clear();
    }

    void clear()
    {
        allocationMap.reset();
        allocatedBlocks.clear();
        memoryPool.clear();
    }

    bool allocate(const T &data)
    {
        size_t size = data.data().size();
        size_t blocksNeeded = (size + BLOCK_SIZE - 1) / BLOCK_SIZE; // Round up to nearest block

        for (size_t i = 0; i <= MAX_BLOCKS - blocksNeeded; ++i)
        {
            bool found = true;
            for (size_t j = 0; j < blocksNeeded; ++j)
            {
                if (allocationMap[i + j])
                {
                    found = false;
                    break;
                }
            }

            if (found)
            {
                for (size_t j = 0; j < blocksNeeded; ++j)
                {
                    allocationMap.set(i + j);
                }

                uint8_t *memStart = memoryPool.data() + (i * BLOCK_SIZE);
                etl::span<uint8_t> newSpan(memStart, size);
                std::copy(data.data().begin(), data.data().end(), memStart);
                
                // Update data to reference its new memory location
                T newBlock = data;
                newBlock.data(newSpan);
                allocatedBlocks.push_back(newBlock);
                return true;
            }
        }
        return false; // No free contiguous blocks
    }

    bool deallocate( T &data)
    {
        bool deAllocated = false;
        uintptr_t offset = data.data().data() - memoryPool.data();
        size_t index = offset / BLOCK_SIZE;
        size_t blocksToFree = (data.data().size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
        
        if (index < MAX_BLOCKS)
        {
            for (size_t j = 0; j < blocksToFree; ++j)
            {
                allocationMap.reset(index + j);
            }
            
            for (auto it = allocatedBlocks.begin(); it != allocatedBlocks.end(); ++it)
            {
                if (it->data().data() == data.data().data())
                {
                    allocatedBlocks.erase(it);
                    deAllocated = true;
                    break;
                }
            }
        }
        return deAllocated;
    }

    // Expose iterators for allocatedBlocks
    typename etl::vector<T, MAX_BLOCKS>::iterator begin() { return allocatedBlocks.begin(); }
    typename etl::vector<T, MAX_BLOCKS>::iterator end() { return allocatedBlocks.end(); }
    typename etl::vector<T, MAX_BLOCKS>::const_iterator begin() const { return allocatedBlocks.begin(); }
    typename etl::vector<T, MAX_BLOCKS>::const_iterator end() const { return allocatedBlocks.end(); }
    
    // Optionally, add cbegin and cend for constant iterators
    typename etl::vector<T, MAX_BLOCKS>::const_iterator cbegin() const { return allocatedBlocks.cbegin(); }
    typename etl::vector<T, MAX_BLOCKS>::const_iterator cend() const { return allocatedBlocks.cend(); }

        
    etl::vector<T, MAX_BLOCKS> &getAllocatedBlocks()
    {
        return allocatedBlocks;
    }

    void printAllocationMap() const
    {
        for (size_t i = 0; i < MAX_BLOCKS; ++i)
        {
            putchar(allocationMap[i] ? '1' : '0');
        }
        puts("");
    }
};
