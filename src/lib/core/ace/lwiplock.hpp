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

    LwipLock(const LwipLock&) = delete;
    LwipLock& operator=(const LwipLock&) = delete;
    LwipLock(LwipLock&&) = delete;
    LwipLock& operator=(LwipLock&&) = delete;
};