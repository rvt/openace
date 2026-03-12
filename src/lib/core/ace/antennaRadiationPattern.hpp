#pragma once

#include "messages.hpp"
#include "etl/array.h"


namespace GATAS
{
    /**
     * @brief Helper class to store antenna radiation pattern measurements.
     *
     * @tparam NUM_RADIALS
     */
    template<size_t NUM_RADIALS = 8>
    class AntennaRadiationPattern
    {
        struct Measurement
        {
            int16_t avgRssiDbm;
            int16_t maxRssiDbm; // the rssi that was received by maxDistance
            uint32_t avgDistance;
            uint32_t maxDistance;
            Measurement() : avgRssiDbm(-128), maxRssiDbm(-128), avgDistance(0), maxDistance(0)
            {
            }

             Measurement(const Measurement&) = delete;
             Measurement& operator=(const Measurement&) = delete;
             Measurement(Measurement&&) = delete;
             Measurement& operator=(Measurement&&) = delete;
        };

    private:
        etl::array<Measurement, NUM_RADIALS> radiationPattern;

    public:
        AntennaRadiationPattern() : radiationPattern() {};


        void put(const GATAS::AircraftPositionMsg &msg) {
            auto &position = msg.position;
            uint8_t positionInRadial = CoreUtils::getRadialSection<NUM_RADIALS>(CoreUtils::bearingFromInDegShort(position.relEastFromOwn, position.relNorthFromOwn));
            Measurement &measurement = radiationPattern[positionInRadial];

            if (position.distanceFromOwn > measurement.maxDistance)
            {
                measurement.maxDistance = position.distanceFromOwn;
                measurement.maxRssiDbm = msg.rssidBm;
            }
            measurement.avgDistance = (measurement.avgDistance + position.distanceFromOwn) / 2;
            measurement.avgRssiDbm = (measurement.avgRssiDbm + msg.rssidBm) / 2;
        }

        void serialize(etl::string_stream &stream) const {
            stream << "[";

            // [ [avgRssiDbm, maxRssiDbm, avgDistance, maxDistance], [avgRssiDbm, maxRssiDbm, avgDistance, maxDistance], ....]
            for (size_t i = 0; i < NUM_RADIALS; ++i) {
                const Measurement &m = radiationPattern[i];

                stream << "[" << m.avgRssiDbm << ","
                << m.maxRssiDbm << ","
                << m.avgDistance << ","
                << m.maxDistance << "]";

                if (i < NUM_RADIALS - 1) {
                    stream << ",";
                }
            }

            stream << "]";
        }

    };

}
