#pragma once

#ifndef NDEBUG
    #define OPENACE_ASSERT(condition, message)                          \
    if (!(condition)) {                                                 \
        printf("Assertion failed: %s\nFile: %s\nLine: %d\n",            \
               (message), __FILE__, __LINE__);                          \
        panic("Assertion");                                             \
    }    
#else
    #define OPENACE_ASSERT(condition, message) do {} while (false)
#endif
