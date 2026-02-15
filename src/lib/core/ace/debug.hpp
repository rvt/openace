#pragma once

#if GATAS_DEBUG

static constexpr const char *basename(const char *path)
{
    const char *last = path;
    for (const char *p = path; *p; ++p)
    {
        if (*p == '/' || *p == '\\')
            last = p + 1;
    }
    return last;
}

#define GATAS_INFO(fmt, ...)                                    \
    do                                                          \
    {                                                           \
        printf("(%s:%d) INFO: ", basename(__FILE__), __LINE__); \
        printf(fmt, ##__VA_ARGS__);                             \
        putchar('\n');                                          \
    } while (0)

#define GATAS_WARNING(fmt, ...)                                    \
    do                                                             \
    {                                                              \
        printf("(%s:%d) WARNING: ", basename(__FILE__), __LINE__); \
        printf(fmt, ##__VA_ARGS__);                                \
        putchar('\n');                                             \
    } while (0)

#define GATAS_LOG_IF(mask, fmt, ...)                                \
    do                                                              \
    {                                                               \
        if (GATAS_LOG_ACTIVE_MODULES & (mask))                      \
        {                                                           \
            printf("(%s:%d) INFO: ", basename(__FILE__), __LINE__); \
            printf(fmt, ##__VA_ARGS__);                             \
            putchar('\n');                                          \
        }                                                           \
    } while (0)

#define GATAS_ASSERT(cond, fmt, ...)                                  \
    do                                                                \
    {                                                                 \
        if (!(cond))                                                  \
        {                                                             \
            printf("(%s:%d) ASSERT: ", basename(__FILE__), __LINE__); \
            printf(fmt, ##__VA_ARGS__);                               \
            panic("Assertion");                                       \
        }                                                             \
    } while (0)

#define GATAS_VERIFY(cond, msg)                                                       \
    do                                                                                \
    {                                                                                 \
        if (!(cond))                                                                  \
        {                                                                             \
            printf("VERIFY FAILED: %s (%s:%d)\n", msg, basename(__FILE__), __LINE__); \
        }                                                                             \
    } while (0)

#else

#define GATAS_INFO(fmt, ...) ((void)0)
#define GATAS_WARNING(fmt, ...) ((void)0)
#define GATAS_ASSERT(cond, msg) ((void)0)
#define GATAS_VERIFY(cond, msg) ((void)0)
#define GATAS_LOG_IF(mask, fmt, ...) ((void)0)

#endif
