#pragma once

#include <stdint.h>
#include "pico/sync.h"

/**
 * @brief Classic Guard based on spinlock
 * Usecase if you now the lock will be very short, like copying of data from configurations changes
 * Don't use this for resources, lock is fast, but should happen only for microseconds
 */
class SpinlockGuard {
private:
    spin_lock_t* lock;
    uint32_t save;

public:
    explicit SpinlockGuard(uint32_t lock_num)
        : lock(spin_lock_instance(lock_num)),
          save(spin_lock_blocking(lock)) {}

    ~SpinlockGuard() {
        spin_unlock(lock, save);
    }

    SpinlockGuard(const SpinlockGuard &) = delete;
    SpinlockGuard &operator=(const SpinlockGuard &) = delete;
    SpinlockGuard(SpinlockGuard &&) = delete;
    SpinlockGuard &operator=(SpinlockGuard &&) = delete;

    operator bool() const
    {
        return true;
    }
};

