#pragma once

#include <etl/atomic.h>

/**
 * @brief Experimental Single Producer Single Consumer (SPSC) lock-free protection of an object.
 * 
 */
template <typename T>
class SafeSPSC {
public:
    SafeSPSC() : seq(0) {
        //static_assert(std::atomic<T>::is_lock_free, "T must be lock-free for atomic access.");
        GATAS_ASSERT(etl::is_trivially_copyable<T>::value, "T must be trivially copyable.");
    }

    void store(const T& value) {
        // Can we use seq++??
        seq.fetch_add(1, etl::memory_order_release);     // mark write in-progress (odd)
        buffer = value;  // store new value
        seq.fetch_add(1, etl::memory_order_release);     // mark write complete (even)
    }

    T load() {
        uint32_t start, end;
        T out;
        do {
            start = seq.load(etl::memory_order_acquire);
            out = buffer;
            end = seq.load(etl::memory_order_acquire);
        } while (start != end || (start & 1)); // retry if write in progress

        return out;
    }

private:
    std::atomic_int foo;
  
    etl::atomic<uint32_t> seq;
    T buffer;
};