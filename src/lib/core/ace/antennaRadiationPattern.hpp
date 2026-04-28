#pragma once

#include "coreutils.hpp"
#include "messages.hpp"
#include "etl/array.h"

namespace GATAS
{
    /**
     * @brief Helper class to store antenna radiation pattern measurements.
     *
     * @tparam NUM_RADIALS
     */
    template <size_t NUM_RADIALS = 8>
    class AntennaRadiationPattern
    {
    public:
        struct Measurement
        {
            int16_t avgRssiDbm;
            int16_t maxRssiDbm;
            uint32_t avgDistance;
            uint32_t maxDistance;

            Measurement() : avgRssiDbm(-128), maxRssiDbm(-128), avgDistance(0), maxDistance(0)
            {
            }

            Measurement(const Measurement &) = delete;
            Measurement &operator=(const Measurement &) = delete;
            Measurement(Measurement &&) = delete;
            Measurement &operator=(Measurement &&) = delete;
        };

    private:
        etl::array<Measurement, NUM_RADIALS> radiationPattern;

    public:
        AntennaRadiationPattern() : radiationPattern() {};

        void put(const GATAS::IngressAircraftPositionMsg &msg)
        {
            auto &position = msg.position;
            const float bearingFromOwn = CoreUtils::bearingFromInDegShort(position.relEastFromOwn, position.relNorthFromOwn);
            const float relativeBearing = CoreUtils::toBearing(bearingFromOwn - static_cast<float>(position.track));
            uint8_t positionInRadial = CoreUtils::getRadialSection<NUM_RADIALS>(relativeBearing);
            Measurement &measurement = radiationPattern[positionInRadial];

            if (position.distanceFromOwn > measurement.maxDistance)
            {
                measurement.maxDistance = position.distanceFromOwn;
                measurement.maxRssiDbm = msg.rssidBm;
            }
            measurement.avgDistance = (measurement.avgDistance + position.distanceFromOwn) / 2;
            measurement.avgRssiDbm = (measurement.avgRssiDbm + msg.rssidBm) / 2;
        }

        const etl::array<Measurement, NUM_RADIALS>& _radiationPattern() const
        {
            return radiationPattern;
        }

        void serialize(etl::string_stream &stream) const
        {
            // Generates: [ [avgRssiDbm, maxRssiDbm, avgDistance, maxDistance], [avgRssiDbm, maxRssiDbm, avgDistance, maxDistance], ....]
            stream << "[";
            for (size_t i = 0; i < NUM_RADIALS; ++i)
            {
                const Measurement &m = radiationPattern[i];

                stream << "[" << m.avgRssiDbm << ","
                       << m.maxRssiDbm << ","
                       << m.avgDistance << ","
                       << m.maxDistance << "]";

                if (i < NUM_RADIALS - 1)
                {
                    stream << ",";
                }
            }

            stream << "]";
        }
    };

}
