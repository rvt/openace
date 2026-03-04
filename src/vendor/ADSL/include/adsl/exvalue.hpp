#pragma once

#include <stdint.h>
#include "etl/optional.h"

template<typename T>
class ExpiringValue final
{
public:
    using time_ms_t = uint32_t;

    /// expireAfterMs = validity window in milliseconds
    explicit ExpiringValue(time_ms_t expireAfterMs)
        : expireAfterMs_(expireAfterMs)
    {}

    /// Set value at a given time (absolute timestamp in ms)
    void set(const T& value, time_ms_t nowMs)
    {
        value_      = value;
        setTimeMs_  = nowMs;
        hasValue_   = true;
    }

    /// Clear value manually
    void clear()
    {
        hasValue_ = false;
    }

    /// Get value if not expired
    etl::optional<T> get(time_ms_t nowMs) const
    {
        if (!hasValue_)
            return etl::nullopt;

        if (isExpired(nowMs))
            return etl::nullopt;

        return value_;
    }

    /// Check validity without copying
    bool valid(time_ms_t nowMs) const
    {
        if (!hasValue_)
            return false;

        return !isExpired(nowMs);
    }

    /// Age in ms (only valid if hasValue())
    time_ms_t age(time_ms_t nowMs) const
    {
        if (!hasValue_)
            return 0;

        return nowMs - setTimeMs_;
    }

    /// How long until it expires (0 if already expired or empty)
    time_ms_t timeLeft(time_ms_t nowMs) const
    {
        if (!hasValue_)
            return 0;

        time_ms_t elapsed = nowMs - setTimeMs_;
        if (elapsed >= expireAfterMs_)
            return 0;

        return expireAfterMs_ - elapsed;
    }

private:
    bool isExpired(time_ms_t nowMs) const
    {
        // Wraparound-safe unsigned arithmetic
        return (nowMs - setTimeMs_) >= expireAfterMs_;
    }

private:
    T value_{};
    time_ms_t setTimeMs_{0};
    time_ms_t expireAfterMs_{0};
    bool hasValue_{false};
};
