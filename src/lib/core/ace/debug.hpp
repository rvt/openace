#pragma once

#if GATAS_DEBUG

static constexpr const char *basename(const char *path)
{
    const char *last = path;
    for (const char *p = path; *p; ++p)
        if (*p == '/' || *p == '\\') last = p + 1;
    return last;
}

#define GATAS_INFO(fmt, ...) \
    printf("\033[01;00m (%s:%d) INFO: " fmt "\n", basename(__FILE__), __LINE__, ##__VA_ARGS__)

#define GATAS_WARN(fmt, ...) \
    printf("\033[01;33m (%s:%d) WARN: " fmt "\n", basename(__FILE__), __LINE__, ##__VA_ARGS__)

#define GATAS_LOG_IF(mask, fmt, ...)                                  \
    do                                                                \
    {                                                                 \
        if (GATAS_LOG_ACTIVE_MODULES & (mask))                        \
            printf("\033[01;30m (%s:%d) INFO: " fmt "\n",                         \
                   basename(__FILE__), __LINE__, ##__VA_ARGS__);     \
    } while (0)

#define GATAS_ASSERT(cond, fmt, ...)                                  \
    do                                                                \
    {                                                                 \
        if (!(cond))                                                  \
        {                                                             \
            printf("\033[01;31m (%s:%d) ASSERT: " fmt "\n",                       \
                   basename(__FILE__), __LINE__, ##__VA_ARGS__);     \
            panic("Assertion");                                       \
        }                                                             \
    } while (0)

#define GATAS_VERIFY(cond, msg)                                       \
    do                                                                \
    {                                                                 \
        if (!(cond))                                                  \
            printf("\033[33m VERIFY: %s (%s:%d)\n",                     \
                   msg, basename(__FILE__), __LINE__);                \
    } while (0)

#else

#define GATAS_INFO(...)        ((void)0)
#define GATAS_WARN(...)        ((void)0)
#define GATAS_ASSERT(...)      ((void)0)
#define GATAS_VERIFY(...)      ((void)0)
#define GATAS_LOG_IF(...)      ((void)0)

#endif