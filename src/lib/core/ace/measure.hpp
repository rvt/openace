#pragma once

#include "pico/time.h"
#include "etl/string_view.h"
#include "coreutils.hpp"
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
    const uint32_t id_ = 0xFFFFFFFF; 
    const uint32_t alertTimeout_ = 1000;
    const etl::string_view name_ = "Took";
    Measure() : start_(CoreUtils::timeUs32Raw()) {}
    Measure(const etl::string_view name) : start_(CoreUtils::timeUs32Raw()), name_(name) {}
    Measure(const etl::string_view name, uint32_t alertTimeoutUs) : start_(CoreUtils::timeUs32Raw()), alertTimeout_(alertTimeoutUs), name_(name) {}
    Measure(const etl::string_view name, uint32_t alertTimeoutUs, uint32_t id) : start_(CoreUtils::timeUs32Raw()), id_(id), alertTimeout_(alertTimeoutUs), name_(name) {}
    ~Measure()
    {
        uint32_t duration = static_cast<uint32_t>(CoreUtils::timeUs32Raw() - start_);
        if (duration > alertTimeout_)
        {
            if (id_ == 0xFFFFFFFF) {
                printf("%s %" PRIu32 "us\n", name_.begin(), duration);
            } else {
                printf("%s %" PRIu32 "us id:%08" PRIX32 "\n", name_.begin(), duration, id_);
            }
        }
    }

    Measure(const Measure &) = delete;
    Measure &operator=(const Measure &) = delete;

    operator bool() const
    {
        uint32_t duration = static_cast<uint32_t>(CoreUtils::timeUs32Raw() - start_);
        return duration > alertTimeout_;
    }
};
#else
struct Measure
{
    Measure() {}
    Measure(const etl::string_view) {}
    Measure(const etl::string_view, uint32_t) {}
    Measure(const etl::string_view, uint32_t, uint32_t) {}

    ~Measure() {}

    Measure(const Measure &) = delete;
    Measure &operator=(const Measure &) = delete;

    operator bool() const
    {
        return false;
    }

};
#endif