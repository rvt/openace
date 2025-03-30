#pragma once

#include "etl/string_view.h"

struct Measure
{
    const uint32_t start;
    const etl::string_view name = "Took";
    Measure() : start(CoreUtils::timeUs32()) {}
    Measure(const etl::string_view name_) : start(CoreUtils::timeUs32()), name(name_) {}
    ~Measure()
    {
        printf("%s %ld us\n", name.begin(), (uint32_t)(CoreUtils::timeUs32() - start));
    }

    Measure(const Measure &) = delete;
    Measure &operator=(const Measure &) = delete;

    operator bool() const
    {
        return true;
    }
};