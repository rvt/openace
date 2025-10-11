#pragma once

#if GATAS_DEBUG == 1
    #define GATAS_ASSERT(condition, message)                          \
    if (!(condition)) {                                                 \
        printf("Assertion failed: %s\nFile: %s\nLine: %d\n",            \
               (message), __FILE__, __LINE__);                          \
        panic("Assertion");                                             \
    }    
#else
    #define GATAS_ASSERT(condition, message) 
#endif
