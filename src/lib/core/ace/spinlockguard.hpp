#pragma once

#include <stdint.h>
#include "pico/sync.h"

/**
 * @brief Classic Guard based on spinlock
 * Usecase if you now the lock will be very short, like copying of data from configurations changes
 * Don't use this for resources, lock is fast, but should happen only for microseconds
 */
class SpinlockGuard
{
private:
    spin_lock_t *lock;
    uint32_t save;

public:
    explicit SpinlockGuard(int lock_num)
        : lock(spin_lock_instance(lock_num)),
          save(spin_lock_blocking(lock)) {}

    ~SpinlockGuard()
    {
        spin_unlock(lock, save);
    }

    /**
     * Request a spinlock. When required is set to value, the function won't panic if no spinlock is available.
     */
    static int claim(bool required = true)
    {
        return spin_lock_claim_unused(required);
    }

    SpinlockGuard(const SpinlockGuard &) = delete;
    SpinlockGuard &operator=(const SpinlockGuard &) = delete;
    SpinlockGuard(SpinlockGuard &&) = delete;
    SpinlockGuard &operator=(SpinlockGuard &&) = delete;

    template <typename F>
    inline static auto withLock(int lock, F &&fn)
    {
        SpinlockGuard guard(lock);
        return fn;
    }

    operator bool() const
    {
        return true;
    }
};
