#pragma once

#include <etl/pool.h>
#include <etl/tuple.h>
#include <etl/utility.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "semaphoreguard.hpp"

#include <cstring>

template <typename Pool, typename T>
class PoolReleaseGuard
{
public:
    PoolReleaseGuard(Pool &pool, T *&ptr)
        : pool_(&pool), ptr_(&ptr)
    {
    }

    ~PoolReleaseGuard()
    {
        if (ptr_ && *ptr_)
        {
            pool_->release(*ptr_);
            *ptr_ = nullptr; // poison the original owner
        }
    }

    void disarm()
    {
        ptr_ = nullptr; // stops destructor from touching anything
    }

    PoolReleaseGuard(const PoolReleaseGuard &) = delete;
    PoolReleaseGuard &operator=(const PoolReleaseGuard &) = delete;

private:
    Pool *pool_;
    T **ptr_;
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

    using PoolTuple = etl::tuple<PoolWrapper<Pools>...>;
    PoolTuple pools;
    SemaphoreHandle_t mutex;

public:
    MultiPoolAllocator()
    {
        mutex = xSemaphoreCreateMutex();
        GATAS_ASSERT(mutex != nullptr, "Failed to create MultiPoolAllocator mutex");
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
#if UINTPTR_MAX == 0xFFFFFFFF
        auto masked_ptr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) & ~0x3U);
#else
        auto masked_ptr = ptr;
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

    // Note to self: problem with the unique_ptr is that we need to change the wrapping objects also to unique_ptr
    // If we do that, we need a pool for that so we can just move pointers around. In the prvious refactor that was to large
    // to handle

    // struct NoopDeleter
    // {
    //     template <typename T>
    //     void operator()(T *) const noexcept {}
    // };

    // template <typename T = uint8_t>
    // using PoolUniquePtr = std::unique_ptr<T, NoopDeleter>;

    // template <typename T = uint8_t>
    // constexpr PoolUniquePtr<T> make_null_pool_ptr()
    // {
    //     return PoolUniquePtr<T>(nullptr, NoopDeleter{});
    // }

    // template <typename T = uint8_t>
    // PoolUniquePtr<T> unique_alloc(size_t size)
    // {
    //     void *raw = alloc(size);
    //     if (!raw)
    //     {
    //         return PoolUniquePtr<T>(nullptr, +[](T *) {});
    //     }

    //     return PoolUniquePtr<T>(
    //         static_cast<T *>(raw),
    //         +[this](T *p)
    //         {
    //             this.release(p);
    //         });
    // }

    // template <typename T = uint8_t>
    // PoolUniquePtr<T> unique_realloc(PoolUniquePtr<T> &uptr, size_t newSize)
    // {
    //     // If the current pointer is null, just allocate a new block
    //     if (!uptr)
    //     {
    //         void *raw = alloc(newSize);
    //         if (!raw)
    //         {
    //             return PoolUniquePtr<T>(nullptr, +[](T *) {});
    //         }

    //         return PoolUniquePtr<T>(
    //             static_cast<T *>(raw),
    //             +[this](T *p)
    //             { this->release(p); });
    //     }

    //     // realloc the existing block
    //     void *newPtr = realloc(uptr.get(), newSize);
    //     if (!newPtr)
    //     {
    //         return PoolUniquePtr<T>(nullptr, +[](T *) {});
    //     }

    //     // release the old pointer (automatic via uptr reset)
    //     uptr.release(); // release ownership, we now manage newPtr

    //     return PoolUniquePtr<T>(
    //         static_cast<T *>(newPtr),
    //         +[this](T *p)
    //         { this->release(p); });
    // }

    // template <typename T = uint8_t>
    // PoolUniquePtr<T> wrap_unique(T *ptr)
    // {
    //     if (!ptr)
    //     {
    //         return PoolUniquePtr<T>(nullptr, +[](T *) {});
    //     }

    //     // This is a serious bug: wrapping memory not owned by this allocator
    //     GATAS_ASSERT(owns_pool(ptr), "Attempting to wrap pointer not owned by any pool");

    //     return PoolUniquePtr<T>(
    //         ptr,
    //         +[this](T *p)
    //         {
    //             this->release(p);
    //         });
    // }

    // Note to self:
    // Problem with teh shared_ptr is that it consumes quite a bit of memopry compared to the data it is managing
    // template <typename T = uint8_t>
    // std::shared_ptr<T> wrap_shared(T *ptr)
    // {
    //     if (!ptr)
    //     {
    //         return std::shared_ptr<T>(nullptr, +[](T *) {});
    //     }

    //     // Assert that pointer is owned by this allocator
    //     // GATAS_ASSERT(owns_pool(ptr), "Attempting to wrap pointer not owned by any pool");

    //     return std::shared_ptr<T>(
    //         ptr,
    //         [this](T *p)
    //         {
    //             this->release(p);
    //         });
    // }

private:
    template <size_t I>
    void *alloc_impl(size_t size)
    {
        if constexpr (I >= sizeof...(Pools))
        {
            return nullptr;
        }
        else
        {
            using PoolT = typename etl::tuple_element<I, PoolTuple>::type;
            if (size <= PoolT::block_size)
            {
                auto ptr = etl::get<I>(pools).allocate();
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
