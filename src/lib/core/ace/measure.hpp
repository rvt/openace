#pragma once

#include "pico/time.h"
#include "etl/string_view.h"
#include "coreutils.hpp"

#ifndef NDEBUG

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
    const uint32_t id_ = 0;
    uint32_t alertTimeout_ = 5000;
    const etl::string_view name_ = "Took";
    Measure() : start_(CoreUtils::timeUs32Raw()) {}
    Measure(const etl::string_view name) : start_(CoreUtils::timeUs32Raw()), name_(name) {}
    Measure(const etl::string_view name, uint32_t alertTimeout) : start_(CoreUtils::timeUs32Raw()), alertTimeout_(alertTimeout), name_(name) {}
    Measure(const etl::string_view name, uint32_t alertTimeout, uint32_t id) : start_(CoreUtils::timeUs32Raw()), id_(id), alertTimeout_(alertTimeout), name_(name) {}
    ~Measure()
    {
        auto duration = CoreUtils::timeUs32Raw() - start_;
        if (duration > alertTimeout_)
        {
            if (id_) {
                printf("%s %6ldus id:%08lX\n", name_.begin(), duration, id_);
            } else {
                printf("%s %6ldus\n", name_.begin(), duration);
            }
        }
    }

    Measure(const Measure &) = delete;
    Measure &operator=(const Measure &) = delete;
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
};
#endif