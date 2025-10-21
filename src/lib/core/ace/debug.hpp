#pragma once

#if GATAS_DEBUG

#define GATAS_LOG(fmt, ...)         \
    do                              \
    {                               \
        printf(fmt, ##__VA_ARGS__); \
    } while (0)

#define GATAS_LOG_IF(mask, fmt, ...)           \
    do                                         \
    {                                          \
        if (GATAS_LOG_ACTIVE_MODULES & (mask)) \
        {                                      \
            printf(fmt "\n", ##__VA_ARGS__);   \
        }                                      \
    } while (0)

#define GATAS_ASSERT(cond, msg)                                             \
    do                                                                      \
    {                                                                       \
        if (!(cond))                                                        \
        {                                                                   \
            printf("ASSERT FAILED: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            panic("Assertion");                                             \
        }                                                                   \
    } while (0)

#define GATAS_VERIFY(cond, msg)                                             \
    do                                                                      \
    {                                                                       \
        if (!(cond))                                                        \
        {                                                                   \
            printf("VERIFY FAILED: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        }                                                                   \
    } while (0)

#else

#define GATAS_LOG(fmt, ...) ((void)0)
#define GATAS_ASSERT(cond, msg) ((void)0)
#define GATAS_VERIFY(cond, msg) ((void)0)
#define GATAS_LOG_IF(mask, fmt, ...) ((void)0)

#endif
