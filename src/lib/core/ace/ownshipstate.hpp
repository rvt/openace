#pragma once

#include <cmath>

#include "etl/map.h"
#include "etl/unordered_map.h"
#include "etl/optional.h"

#include "models.hpp"
#include "spinlockguard.hpp"

// Forward declaration avoids the coreutils/messages/ownshipstate include cycle.
namespace CoreUtils
{
    uint64_t msSinceEpoch();
}

namespace GATAS
{
    struct BarometricPressureSample
    {
        BarometricPressure value{};
        uint64_t msSinceEpoch = 0;
        bool valid = false;
    };

    // Local update metadata only; never part of the pressure model or wire format.
    enum class BarometricSource : uint8_t
    {
        Internal,
        External
    };

    struct OwnshipPressureState
    {
        static constexpr size_t SOURCE_COUNT =
            static_cast<size_t>(PressureSource::PRESSURE_SOURCE_NO_ITEMS);

        etl::unordered_map<
            PressureSource,
            BarometricPressureSample,
            SOURCE_COUNT>
            barometricPressure{};
    };

    /**
     * Inject one instance into producers and consumers. Initialize before use.
     * Pressure updates gate incoming samples before changing the existing method-keyed map.
     * Selection uses one coherent pressure snapshot; location is a separate snapshot.
     */
    class OwnshipState
    {
        SynchronizedValue<OwnshipPressureState> pressure;
        // Arbitration bookkeeping only, accessed inside pressure.update()'s lock.
        // Internal/external origin is not added to the pressure model or samples.
        BarometricSource acceptedSource = BarometricSource::External;

        static bool positiveFinite(float value)
        {
            return std::isfinite(value) && value > 0.0f;
        }

        static uint8_t quality(PressureSource source)
        {
            switch (source)
            {
            case PressureSource::PressureSensor:
                return 2;
            case PressureSource::Calculated:
                return 1;
            default:
                return 0;
            }
        }

        static uint64_t maxAgeMs(PressureSource source)
        {
            switch (source)
            {
            case PressureSource::PressureSensor:
                return 5'000;

            case PressureSource::Calculated:
                return 5'000;

            default:
                return 5'000;
            }
        }

        static bool fresh(
            bool valid,
            uint64_t timestamp,
            uint64_t now,
            PressureSource source)
        {
            // Reject future samples before subtraction, including after a clock correction.
            return valid &&
                   timestamp <= now &&
                   now - timestamp <= maxAgeMs(source);
        }

    public:
        SynchronizedValue<OwnshipPositionInfo> location;

        void updateBarometricPressure(
            BarometricSource origin,
            const BarometricPressure &value,
            uint64_t msSinceEpoch = CoreUtils::msSinceEpoch())
        {
            GATAS_ASSERT(origin == BarometricSource::Internal || origin == BarometricSource::External,
                         "Invalid pressure origin");
            GATAS_ASSERT(static_cast<uint8_t>(value.source) <
                             static_cast<uint8_t>(PressureSource::PRESSURE_SOURCE_NO_ITEMS),
                         "Invalid pressure source");
            pressure.update(
                [&](OwnshipPressureState &state)
                {
                    const BarometricPressureSample sample{
                        value,
                        msSinceEpoch,
                        positiveFinite(value.pressurehPa)};

                    if (!state.barometricPressure.empty())
                    {
                        const auto &current = state.barometricPressure.begin()->second;
                        if (fresh(current.valid, current.msSinceEpoch, msSinceEpoch, current.value.source))
                        {
                            // A missing value cannot displace another producer's
                            // valid data. The accepted origin/method may invalidate itself.
                            if (!sample.valid &&
                                (origin != acceptedSource || value.source != current.value.source))
                            {
                                return;
                            }
                            // Origin wins first; compare methods only for the same origin.
                            if ((acceptedSource == BarometricSource::Internal && origin == BarometricSource::External) ||
                                (origin == acceptedSource && quality(current.value.source) > quality(value.source)))
                            {
                                return;
                            }
                        }
                    }

                    // Keep only the accepted sample, keyed by its measured/calculated
                    // method. Rejected updates do not alter its value or freshness.
                    state.barometricPressure.clear();
                    state.barometricPressure.insert(etl::make_pair(value.source, sample));
                    acceptedSource = origin;
                });
        }

        OwnshipPressureState loadPressureState() const
        {
            return pressure.load();
        }

        // Returns pressure altitude in metres from the best fresh ambient pressure.
        etl::optional<float> calculatePressureAltitude(
            uint64_t nowMsSinceEpoch = CoreUtils::msSinceEpoch()) const
        {
            const auto snapshot = pressure.load();

            // All priority decisions happen in updateBarometricPressure().
            for (const auto &entry : snapshot.barometricPressure)
            {
                const auto &sample = entry.second;
                if (fresh(sample.valid, sample.msSinceEpoch, nowMsSinceEpoch, sample.value.source))
                {
                    // Pressure altitude is the height corresponding to ambient
                    // pressure in the standard atmosphere, referenced to 1013.25 hPa.
                    // Invert p = p0 * (1 - h / 44330)^(1 / 0.190295):
                    // h = 44330 * (1 - (p / 1013.25)^0.190295).
                    // QNH, GPS height and measured temperature are not required.
                    // The server has already used QNH when estimating ambient pressure.
                    // Pressure above 1013.25 hPa correctly gives negative altitude.
                    // This tropospheric approximation applies below approximately 11 km.

                    return 44330.0f * (1.0f - powf(sample.value.pressurehPa / 1013.25f, 0.190295f));
                }
            }

            // No fresh pressure: GPS height alone is not pressure altitude.
            return etl::nullopt;
        }

        void init(spin_lock_t *lock)
        {
            location.init(lock);
            pressure.init(lock);
        }
    };
}