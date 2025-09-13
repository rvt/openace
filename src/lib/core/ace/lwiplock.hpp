#pragma once

#include "pico/cyw43_arch.h"

/* LwIP */
class LwipLock {
public:
    LwipLock() {
        cyw43_arch_lwip_begin();
    }

    ~LwipLock() {
        cyw43_arch_lwip_end();
    }

    // Non-copyable
    LwipLock(const LwipLock&) = delete;
    LwipLock& operator=(const LwipLock&) = delete;

    // Movable if you want
    LwipLock(LwipLock&&) = delete;
    LwipLock& operator=(LwipLock&&) = delete;
};