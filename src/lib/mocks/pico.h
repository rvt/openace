#pragma once

#include <stdint.h>

typedef unsigned int uint;




// mock panic
inline void panic_unsupported(const char *file, int line, const char *func, const char *msg)
{
    printf("panic_unsupported: %s:%d %s: %s\n", file, line, func, msg);
}


inline void panic(const char *msg)
{
    printf("panic: %s\n", msg);
}

#define __time_critical_func(func_name) func_name
