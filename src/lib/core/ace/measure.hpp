#pragma once

#include "pico/time.h"
#include "etl/string_view.h"
#include "coreutils.hpp"
#include "debug.hpp"
#include <inttypes.h>

#if GATAS_DEBUG == 1

/**
 * @class Measure
 * @brief A lightweight RAII-based performance profiler for tracking code block execution time.
 *
 * This class measures the time between its construction and destruction (scope exit).
 * If the measured duration exceeds a configurable threshold, it logs a warning.
 * Designed for debug builds only (no overhead in release builds).
 *
 * Key Features:
 * - Zero runtime cost in release builds (compiles to empty struct).
 * - Supports custom timeout thresholds, names, and IDs for identification.
 * - Non-copyable/non-movable to ensure accurate timing.
 * - Logs output via `printf` when the timeout is exceeded.
 *
 * Usage:
 *   {
 *       Measure m("SlowFunction");  // Start timer (logs if >5ms)
 *       // ... code being measured ...
 *   }  // Timer stops and logs here if threshold exceeded
 */
struct Measure
{
    const uint32_t start_;
    const etl::string_view name_;
    const uint32_t alertTimeout_;
    const uint32_t id_;
    const char *file_;
    const uint32_t line_;

    Measure(const etl::string_view name,
            const char *file = __FILE__,
            uint32_t line = __LINE__,
            uint32_t alertTimeoutUs = 1000,
            uint32_t id = UINT32_MAX)
        : start_(CoreUtils::timeUs32Raw()),
          name_(name),
          alertTimeout_(alertTimeoutUs),
          id_(id),
          file_(basename(file)),
          line_(line)
    {
    }

    ~Measure()
    {
        uint32_t duration = CoreUtils::timeUs32Raw() - start_;
        if (duration > alertTimeout_)
        {
            if (id_ == UINT32_MAX)
            {
                printf("(%s:%" PRIu32 ") %s %" PRIu32 "us\n", file_, line_, name_.begin(), duration);
            }
            else
            {
                printf("(%s:%" PRIu32 ") %s%" PRIu32 " %" PRIu32 "us\n", file_, line_, name_.begin(), id_, duration);
            }
        }
    }

    Measure(const Measure &) = delete;
    Measure &operator=(const Measure &) = delete;

    operator bool() const
    {
        return (CoreUtils::timeUs32Raw() - start_) > alertTimeout_;
    }
};

#define MEASURE_CONCAT(a, b) MEASURE_CONCAT_INNER(a, b)
#define MEASURE_CONCAT_INNER(a, b) a##b
#define MEASURE_UNIQUE_NAME(base) MEASURE_CONCAT(base, __COUNTER__)

/**
 * Macro to create a Measure instance with a unique name.
 */
#define GATAS_MEASURE(name, ...) \
    auto MEASURE_UNIQUE_NAME(measure) = Measure { name, __FILE__, __LINE__, ##__VA_ARGS__ }

/**
 * Macro to create a Measure instance with a variable 'measure' name.
 */
#define GATAS_MEASURE_M(name, ...) \
    auto measure = Measure { name, __FILE__, __LINE__, ##__VA_ARGS__ }

#else

#define GATAS_MEASURE(name, ...) (void(0))
#define GATAS_MEASURE_M(name, ...) auto measure = false

#endif