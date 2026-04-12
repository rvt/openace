#pragma once

#include <etl/pool.h>
#include <etl/tuple.h>
#include <etl/utility.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "semaphoreguard.hpp"
#include <cstddef> // For max_alignas_t
#include <cstring>

template <typename Pool, typename T>
class PoolOwnedPtr
{
public:
    PoolOwnedPtr() = default;

    PoolOwnedPtr(Pool &pool, T *ptr) : pool_(&pool), ptr_(ptr)
    {
    }

    ~PoolOwnedPtr()
    {
        reset();
    }

    PoolOwnedPtr(const PoolOwnedPtr &) = delete;
    PoolOwnedPtr &operator=(const PoolOwnedPtr &) = delete;

    PoolOwnedPtr(PoolOwnedPtr &&other) noexcept : pool_(other.pool_), ptr_(other.ptr_)
    {
        other.pool_ = nullptr;
        other.ptr_ = nullptr;
    }

    PoolOwnedPtr &operator=(PoolOwnedPtr &&other) noexcept
    {
        if (this != &other)
        {
            reset();
            pool_ = other.pool_;
            ptr_ = other.ptr_;
            other.pool_ = nullptr;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    void reset(T *newPtr = nullptr)
    {
        if (ptr_ && pool_)
        {
            pool_->release(ptr_);
        }

        ptr_ = newPtr;
    }

    void adopt(Pool &pool, T *ptr)
    {
        reset();
        pool_ = &pool;
        ptr_ = ptr;
    }

    T *detach() const
    {
        T *ptr = ptr_;
        ptr_ = nullptr;
        return ptr;
    }

    T *get() const
    {
        return ptr_;
    }

    operator T *() const
    {
        return ptr_;
    }

    T &operator*() const
    {
        return *ptr_;
    }

    T *operator->() const
    {
        return ptr_;
    }

    explicit operator bool() const
    {
        return ptr_ != nullptr;
    }

private:
    mutable Pool *pool_ = nullptr;
    mutable T *ptr_ = nullptr;
};

// Pool specification
template <size_t BlockSize, size_t Count>
struct PoolSpec
{
    static_assert(BlockSize % sizeof(uint32_t) == 0, "Pool block size must be multiple of 4 bytes (uint32_t)");
    static constexpr size_t block_size = BlockSize;
    static constexpr size_t count = Count;
};

// Raw memory block wrapper for ETL pool
template <size_t N>
struct PoolBlock
{
    alignas(std::max_align_t) uint8_t data[N];
};

// Pool wrapper
template <typename Spec>
struct PoolWrapper
{
    static constexpr size_t block_size = Spec::block_size;
    static constexpr size_t count = Spec::count;

    using block_t = PoolBlock<block_size>;
    etl::pool<block_t, count> pool;

    void *allocate()
    {
        block_t *b = pool.allocate();
        return b ? static_cast<void *>(b->data) : nullptr;
    }

    bool owns(const void *p) const
    {
        if (!p)
            return false;

        auto *block = reinterpret_cast<const block_t *>(reinterpret_cast<const uint8_t *>(p) - offsetof(block_t, data));
        return pool.is_in_pool(block);
    }

    void release(void *p)
    {
        if (!p)
            return;

        auto *block = reinterpret_cast<block_t *>(reinterpret_cast<uint8_t *>(p) - offsetof(block_t, data));
        pool.release(block);
    }
};

// Multi-pool allocator
template <typename... Pools>
class MultiPoolAllocator
{
    static_assert(sizeof...(Pools) >= 2, "Need at least 2 pools");
    static constexpr size_t max_block_size = std::max({Pools::block_size...});

    using PoolTuple = etl::tuple<PoolWrapper<Pools>...>;
    PoolTuple pools;
    SemaphoreHandle_t mutex;

public:
    MultiPoolAllocator()
    {
        mutex = xSemaphoreCreateMutex();
        GATAS_ASSERT(mutex != nullptr, "Failed to create MultiPoolAllocator mutex");
    }

    constexpr size_t maxPoolSize() const {
        return max_block_size;
    }

    void *alloc(size_t size)
    {
        SemaphoreGuard lock(portMAX_DELAY, mutex);
        return alloc_impl<0>(size);
    }

    void release(void *ptr)
    {
#if UINTPTR_MAX == 0xFFFFFFFF
        // For 32-bit targets, we need to mask off the lower 2 bits to get the actual block pointer
        // this was needed for ADSL that would return a pointer to a byte not alligned
        ptr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) & ~0x3U);
#endif
        SemaphoreGuard lock(portMAX_DELAY, mutex);
        release_impl<0>(ptr);
    }

    void release(const void *ptr)
    {
// Added to enable testing of the class
#if UINTPTR_MAX == 0xFFFFFFFF
        auto masked_ptr = reinterpret_cast<void *>(reinterpret_cast<const uintptr_t>(ptr) & ~0x3U);
#else
        auto masked_ptr = const_cast<void*>(ptr);
#endif
        SemaphoreGuard lock(portMAX_DELAY, mutex);
        release_impl<0>(masked_ptr);
    }

    void *realloc(void *ptr, size_t newSize)
    {
        if (!ptr)
        {
            return alloc(newSize);
        }

        SemaphoreGuard lock(portMAX_DELAY, mutex);
        return realloc_impl<0>(ptr, newSize);
    }

    // Test helper
    template <size_t I>
    bool owns_pool(const void *ptr) const
    {
        SemaphoreGuard lock(portMAX_DELAY, mutex);
        return etl::get<I>(pools).owns(ptr);
    }

private:
    template <size_t I>
    void *alloc_impl(size_t size)
    {
        if constexpr (I >= sizeof...(Pools))
        {
            GATAS_WARN("Alloc to large");
            return nullptr;
        }
        else
        {
            using PoolT = typename etl::tuple_element<I, PoolTuple>::type;
            if (size <= PoolT::block_size)
            {
                auto ptr = etl::get<I>(pools).allocate();
                if (ptr == nullptr)
                {
                    GATAS_WARN("Alloc full");
                }
                return ptr;
                // We could enable this if we want to automatically find a larger pool
                // wheer this fits
                // if (void *p = etl::get<I>(pools).allocate())
                // {
                //   return p;
                // }
            }
            return alloc_impl<I + 1>(size);
        }
    }

    template <size_t I>
    void release_impl(void *ptr)
    {
        if constexpr (I < sizeof...(Pools))
        {
            if (etl::get<I>(pools).owns(ptr))
            {
                etl::get<I>(pools).release(ptr);
                return;
            }
            release_impl<I + 1>(ptr);
        }
    }

    template <size_t I>
    void *realloc_impl(void *ptr, size_t newSize)
    {
        if constexpr (I >= sizeof...(Pools))
        {
            GATAS_WARN("Realloc to large");
            return nullptr;
        }
        else
        {
            using PoolT = typename etl::tuple_element<I, PoolTuple>::type;

            if (etl::get<I>(pools).owns(ptr))
            {
                if (newSize <= PoolT::block_size)
                {
                    return ptr; // still fits
                }
                else
                {
                    void *newPtr = alloc(newSize);
                    if (!newPtr)
                    {
                        GATAS_WARN("Realloc full");
                        etl::get<I>(pools).release(ptr);
                        return nullptr;
                    }

                    std::memcpy(newPtr, ptr, PoolT::block_size);
                    etl::get<I>(pools).release(ptr);
                    return newPtr;
                }
            }
            return realloc_impl<I + 1>(ptr, newSize);
        }
    }
};
