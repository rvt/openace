#pragma once

#include <stdint.h>
#include "../pico.h"


inline uint64_t get_absolute_timeValue = 0;
// us since boot
inline uint64_t get_absolute_time()
{
    return get_absolute_timeValue;
}

inline uint64_t to_us_since_boot(uint64_t time)
{
    return get_absolute_time();
}

inline uint64_t to_ms_since_boot(uint64_t time)
{
    return get_absolute_time() / 1'000;
}

inline uint64_t time_us_64Value= 0;
inline uint64_t time_us_64()
{
    return time_us_64Value;
}

inline uint32_t time_us_32Value= 0;
inline uint32_t time_us_32() {
      return time_us_32Value;  
}