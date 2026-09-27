#pragma once

#include <cmath>
#include "etl/optional.h"

#include "models.hpp"
#include "spinlockguard.hpp"

namespace GATAS
{
    /**
     * Current ownship samples, with independent atomic snapshot reads and writes.
     * Initialize once during startup before sharing this object with modules.
     * Both values use the supplied lock; no hardware spinlock is allocated here.
     * Separate loads do not form a single atomic snapshot of both samples.
     */
    class OwnshipState
    {
    public:
        SynchronizedValue<OwnshipPositionInfo> location;
        SynchronizedValue<BarometricPressure> barometricPressure;
        SynchronizedValue<PressureAltQnh> pressureAltQnh;

        // Returns pressure altitude in metres, using the first available method.
        etl::optional<float> calculatePressureAltitude(bool includeGpsQnhFallback = true) const
        {
            const auto received = pressureAltQnh.load();
            const bool hasReceivedAltitude = received.pressureAlt != INVALID_BARO_ALTITUDE;

            // 1. Prefer pressure altitude supplied by a pressure sensor.
            // It is already referenced to 1013.25 hPa, so QNH is not required.
            if (hasReceivedAltitude && received.source == PressureSource::PressureSensor)
            {
                return static_cast<float>(received.pressureAlt);
            }

            // 2. Calculate from measured ambient pressure (e.g. BMP280).
            // Copy under the lock and calculate after load() releases it. The
            // sensor must sample representative static air pressure.
            const float pressureHpa = barometricPressure.load().pressurehPa;
            if (std::isfinite(pressureHpa) && pressureHpa > 0.0f)
            {
                // Pressure altitude is the altitude corresponding to measured
                // pressure in the standard atmosphere, referenced to 1013.25 hPa.
                // Neither QNH, GPS height nor measured temperature is required.
                // Invert the standard tropospheric pressure-height relationship:
                // h = (T0 / L) * (1 - (p / p0)^(R * L / g)).
                // T0 / L is approximately 44330 metres; R * L / g is 0.190295.
                // Pressure above 1013.25 hPa correctly gives negative altitude.
                // This tropospheric approximation applies below approximately 11 km.
                return 44330.0f * (1.0f - powf(pressureHpa / 1013.25f, 0.190295f));
            }

            // 3. Use another received pressure altitude, including calculated
            // estimates or values whose source is unavailable.
            // A valid pressure altitude remains usable when QNH is unavailable.
            if (hasReceivedAltitude && received.source == PressureSource::Calculated)
            {
                return static_cast<float>(received.pressureAlt);
            }

            // 4. Estimate from GPS MSL height and local QNH. First infer ambient
            // pressure using p = QNH * (1 - hMSL / 44330)^(1 / 0.190295), then
            // convert that pressure to altitude referenced to 1013.25 hPa.
            // Combining both equations gives the expression below. GPS height is
            // geometric, not barometric, so this assumes a standard temperature
            // profile and representative local QNH; it is our least reliable method.
            if (includeGpsQnhFallback && std::isfinite(received.qnh) && received.qnh > 0.0f)
            {
                const auto position = location.load();
                // There is no fix-valid flag in this snapshot. A nonzero timestamp
                // indicates a stored position, but does not guarantee freshness.
                // Pressure samples also lack timestamps, so no method checks age.
                const float heightMsl = static_cast<float>(position.ellipseHeight) -
                                        static_cast<float>(position.geoidSeparation);
                if (position.timestamp != 0 && heightMsl < 44330.0f)
                {
                    return 44330.0f * (1.0f - powf(received.qnh / 1013.25f, 0.190295f) *
                                              (1.0f - heightMsl / 44330.0f));
                }
            }

            // 5. Unavailable. GPS height alone is not pressure altitude; do not
            // silently return it or assume a QNH of 1013.25 hPa.
            return etl::nullopt;
        }

        void init(spin_lock_t *lock)
        {
            location.init(lock);
            barometricPressure.init(lock);
            pressureAltQnh.init(lock);
        }
    };
}
