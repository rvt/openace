#pragma once

#include "pico/cyw43_arch.h"

/* LwIP */
class LwipRAIILock
{
public:
    LwipRAIILock()
    {
        cyw43_arch_lwip_begin();
    }

    ~LwipRAIILock()
    {
        cyw43_arch_lwip_end();
    }

    LwipRAIILock(const LwipRAIILock &) = delete;
    LwipRAIILock &operator=(const LwipRAIILock &) = delete;
    LwipRAIILock(LwipRAIILock &&) = delete;
    LwipRAIILock &operator=(LwipRAIILock &&) = delete;
};
