#pragma once

#include <stdint.h>
#include "pico/sync.h"
#include "etl/utility.h"

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
    explicit SpinlockGuard(spin_lock_t *lock)
        : lock(lock), save(spin_lock_blocking(lock)) {}

    ~SpinlockGuard()
    {
        spin_unlock(lock, save);
    }

    /**
     * Request a spinlock. When required is set to false, the function won't panic if no spinlock is available.
     */
    static spin_lock_t *claim()
    {
        return spin_lock_instance((uint)spin_lock_claim_unused(true));
    }

    SpinlockGuard(const SpinlockGuard &) = delete;
    SpinlockGuard &operator=(const SpinlockGuard &) = delete;
    SpinlockGuard(SpinlockGuard &&) = delete;
    SpinlockGuard &operator=(SpinlockGuard &&) = delete;

    template <typename F>
    inline static const auto copyWithLock(spin_lock_t *lock, const F &fn)
    {
        SpinlockGuard guard(lock);
        return fn;
    }

    template <typename T1, typename T2>
    inline static const auto copyWithLock(spin_lock_t *lock, const T1 &val1, const T2 &val2)
    {
        SpinlockGuard guard(lock);
        return etl::pair<const T1 &, const T2 &>(val1, val2);
    }

    operator bool() const
    {
        return true;
    }
};
