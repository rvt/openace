#pragma once

#include <stdint.h>
#include <stdio.h>
#include "etl/vector.h"
#include "etl/bitset.h"
#include "etl/span.h"

/**
 * @brief A simple block allocator that allocates blocks from a fixed-size memory pool.
 * 
 * The goal is to have a fixed memory pool and allocate memory blocks dynamically for different sizes
 * ensuring that each allocated object maintains a reference to its memory. The goal was to reduce memory for
 * data blocks that can be of different size. 
 *
 * 
 * @tparam T The type of objects to allocate, needs to have the functions
 * etl::span<uint8_t> data() const {..}
 * @tparam MAX_BLOCKS The maximum number of blocks in the memory pool.
 * @tparam BLOCK_SIZE The size of each block in the memory pool.
 */
template <typename T, size_t MAX_BLOCKS, size_t BLOCK_SIZE>
class BlockAllocator
{
private:
    etl::vector<uint8_t, MAX_BLOCKS * BLOCK_SIZE> memoryPool; // Fixed memory pool
    etl::bitset<MAX_BLOCKS> allocationMap;                    // Bit array for tracking used blocks
    using BLOCK_STORE = etl::vector<T, MAX_BLOCKS>;
    BLOCK_STORE allocatedBlocks;               // Stores allocated objects

public:
    /**
     * @brief Constructor that initializes the block allocator.
     */
    BlockAllocator()
    {
        clear();
    }

    /**
     * @brief Clears the memory pool and resets the allocation map.
     */
    void clear()
    {
        allocationMap.reset();
        allocatedBlocks.clear();
        memoryPool.clear();
    }

    /**
     * @brief Adds a new object to the memory pool.
     * 
     * @param data The object to add.
     * @return True if the object was successfully added, false otherwise.
     */
    bool add(const T &data)
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

    /**
     * @brief Removes an object from the memory pool.
     * 
     * @param data The object to remove.
     * @return True if the object was successfully removed, false otherwise.
     */
    auto remove(typename BLOCK_STORE::iterator it)
    {
        uintptr_t offset = it->data().data() - memoryPool.data();
        size_t index = offset / BLOCK_SIZE;
        size_t blocksToFree = (it->data().size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
        if (index < MAX_BLOCKS)
        {
            for (size_t j = 0; j < blocksToFree; ++j)
            {
                allocationMap.reset(index + j);
            }
    
            it = allocatedBlocks.erase(it); // Erase and return next valid iterator
        }
        
        return it; // Return the next valid iterator
    }

    /**
     * @brief Get an iterator to the beginning of the allocated blocks.
     * @return An iterator to the beginning of the allocated blocks.
     */
    typename etl::vector<T, MAX_BLOCKS>::iterator begin() { return allocatedBlocks.begin(); }

    /**
     * @brief Get an iterator to the end of the allocated blocks.
     * @return An iterator to the end of the allocated blocks.
     */
    typename etl::vector<T, MAX_BLOCKS>::iterator end() { return allocatedBlocks.end(); }

    /**
     * @brief Get a constant iterator to the beginning of the allocated blocks.
     * @return A constant iterator to the beginning of the allocated blocks.
     */
    typename etl::vector<T, MAX_BLOCKS>::const_iterator begin() const { return allocatedBlocks.begin(); }

    /**
     * @brief Get a constant iterator to the end of the allocated blocks.
     * @return A constant iterator to the end of the allocated blocks.
     */
    typename etl::vector<T, MAX_BLOCKS>::const_iterator end() const { return allocatedBlocks.end(); }

    /**
     * @brief Get a constant iterator to the beginning of the allocated blocks.
     * @return A constant iterator to the beginning of the allocated blocks.
     */
    typename etl::vector<T, MAX_BLOCKS>::const_iterator cbegin() const { return allocatedBlocks.cbegin(); }

    /**
     * @brief Get a constant iterator to the end of the allocated blocks.
     * @return A constant iterator to the end of the allocated blocks.
     */
    typename etl::vector<T, MAX_BLOCKS>::const_iterator cend() const { return allocatedBlocks.cend(); }

    /**
     * @brief Get the allocated blocks.
     * @return A reference to the vector of allocated blocks.
     */
    const etl::vector<T, MAX_BLOCKS> &getAllocatedBlocks() const
    {
        return allocatedBlocks;
    }

    /**
     * @brief Print the allocation map to the console.
     */
    void printAllocationMap() const
    {
        for (size_t i = 0; i < MAX_BLOCKS; ++i)
        {
            putchar(allocationMap[i] ? '1' : '0');
        }
        puts("");
    }
};
