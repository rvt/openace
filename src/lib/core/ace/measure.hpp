#pragma once

#include "pico/time.h"
#include "etl/string_view.h"

#ifndef NDEBUG
struct Measure
{
    const uint32_t start_;
    const uint32_t id_ = 0;
    uint32_t alert_ = 5000;
    const etl::string_view name_ = "Took";
    Measure() : start_(time_us_32()) {}
    Measure(const etl::string_view name) : start_(time_us_32()), name_(name) {}
    Measure(const etl::string_view name, uint32_t alert) : start_(time_us_32()), alert_(alert), name_(name) {}
    Measure(const etl::string_view name, uint32_t alert, uint32_t id) : start_(time_us_32()), id_(id), alert_(alert), name_(name) {}
    ~Measure()
    {
        auto duration = time_us_32() - start_;
        if (duration > alert_)
        {
            printf("%s %ldus id:%08lX\n", name_.begin(), duration, id_);
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