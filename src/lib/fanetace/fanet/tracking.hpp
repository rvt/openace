#pragma once

#include <stdint.h>
#include <math.h>
#include "etl/algorithm.h"
#include "header.hpp"

namespace FANET
{
    /**
     * Tracking payload
     * Messagetype : 1
     */
    class TrackingPayload final : public Payloadbase
    {
    public:
        enum class AircraftType : uint8_t
        {
            OTHER = 0,            // 0: Other
            PARAGLIDER = 1,       // 1: Paraglider
            HANGLIDER = 2,        // 2: Hangglider
            BALLOON = 3,          // 3: Balloon
            GLIDER = 4,           // 4: Glider
            POWERED_AIRCRAFT = 5, // 5: Powered Aircraft
            HELICOPTER = 6,       // 6: Helicopter
            UAV = 7               // 7: UAV
        };

    private:
        int32_t latitudeRaw = 0;
        int32_t longitudeRaw = 0;
        uint16_t altitudeRaw = 0;
        bool trackingBit = false;
        AircraftType aircraftTypeRaw = AircraftType::OTHER;
        bool aScaling = false;
        bool sScalingBit = false;
        uint8_t speedRaw = 0;
        bool cScalingBit = false;
        int8_t climbRaw = 0;
        uint8_t groundTrackRaw = 0;
        bool tScalingBit = false;
        int8_t turnRateRaw = 0;
        bool hasTurnRateRaw = false;

    public:
        /**
         * @brief Default constructor.
         */
        explicit TrackingPayload() = default;

        /**
         * @brief Constructor with all fields.
         */
        TrackingPayload(int32_t latitudeRaw_, int32_t longitudeRaw_, uint16_t altitudeRaw_,
                        bool trackingBit_, AircraftType aircraftTypeRaw_,
                        bool aScaling_, bool sScalingBit_, uint8_t speedRaw_,
                        bool cScalingBit_, int8_t climbRaw_, uint8_t groundTrackRaw_,
                        bool tScalingBit_, uint8_t turnRateRaw_)
            : latitudeRaw(latitudeRaw_),
              longitudeRaw(longitudeRaw_),
              altitudeRaw(altitudeRaw_),
              trackingBit(trackingBit_),
              aircraftTypeRaw(aircraftTypeRaw_),
              aScaling(aScaling_),
              sScalingBit(sScalingBit_),
              speedRaw(speedRaw_),
              cScalingBit(cScalingBit_),
              climbRaw(climbRaw_),
              groundTrackRaw(groundTrackRaw_),
              tScalingBit(tScalingBit_),
              turnRateRaw(turnRateRaw_)
        {
        }

        /**
         * @brief Get the message type.
         * @return The message type.
         */
        Header::MessageType type() const
        {
            return Header::MessageType::TRACKING;
        }

        /**
         * @brief Get the latitude in degrees.
         * @return The latitude in degrees.
         */
        float latitude() const
        {
            return ((latitudeRaw << 8) >> 8) / 93206.f;
        }

        /**
         * @brief Get the longitude in degrees.
         * @return The longitude in degrees.
         */
        float longitude() const
        {
            return ((longitudeRaw << 8) >> 8) / 46603.f;
        }

        /**
         * @brief Set the latitude in degrees.
         * @param lat The latitude in degrees.
         * @return Reference to the current object.
         */
        TrackingPayload &latitude(float lat)
        {
            lat = etl::clamp(lat, -90.f, 90.f);
            latitudeRaw = roundf(lat * 93206.0f);
            return *this;
        }

        /**
         * @brief Set the longitude in degrees.
         * @param lon The longitude in degrees.
         * @return Reference to the current object.
         */
        TrackingPayload &longitude(float lon)
        {
            lon = etl::clamp(lon, -180.f, 180.f);
            longitudeRaw = roundf(lon * 46603.0f);
            return *this;
        }

        /**
         * @brief Get the altitude in meters.
         * @return The altitude in meters.
         */
        int16_t altitude() const
        {
            return aScaling ? altitudeRaw << 2 : altitudeRaw;
        }

        /**
         * @brief Set the altitude in meters.
         * @param alt The altitude in meters.
         * @return Reference to the current object.
         */
        TrackingPayload &altitude(int16_t alt)
        {
            alt = etl::clamp(static_cast<int>(alt), 0, 8188);
            if (alt > 2047)
            {
                altitudeRaw = (alt + 2) >> 2;
                aScaling = true;
            }
            else
            {
                altitudeRaw = alt;
            }
            return *this;
        }

        /**
         * @brief Check if this aircraft allows tracking.
         * @return True if tracking is allowed, false otherwise.
         */
        bool tracking() const
        {
            return trackingBit;
        }

        /**
         * @brief Set if this aircraft allows tracking.
         * @param tracking True if tracking is allowed, false otherwise.
         * @return Reference to the current object.
         */
        TrackingPayload &tracking(bool tracking)
        {
            trackingBit = tracking;
            return *this;
        }

        /**
         * @brief Get the type of aircraft.
         * @return The type of aircraft.
         */
        AircraftType aircraftType() const
        {
            return aircraftTypeRaw;
        }

        /**
         * @brief Set the type of aircraft.
         * @param aircraftType The type of aircraft.
         * @return Reference to the current object.
         */
        TrackingPayload &aircraftType(AircraftType aircraftType)
        {
            aircraftTypeRaw = aircraftType;
            return *this;
        }

        /**
         * @brief Get the speed in kilometers per hour.
         * @return The speed in kilometers per hour.
         */
        float speed() const
        {
            return sScalingBit ? speedRaw * 2.5f : speedRaw / 2.f;
        }

        /**
         * @brief Set the speed in kilometers per hour.
         * @param speed The speed in kilometers per hour.
         * @return Reference to the current object.
         */
        TrackingPayload &speed(float speed)
        {
            int32_t speed2 = etl::clamp(static_cast<int>(roundf(speed * 2.0f)), 0, 127 * 5);
            if (speed2 > 127)
            {
                speedRaw = (speed2 + 2) / 5;
                sScalingBit = true;
            }
            else
            {
                speedRaw = speed2;
            }
            return *this;
        }

        /**
         * @brief Get the climb rate in meters per second.
         * @return The climb rate in meters per second.
         */
        float climbRate() const
        {
            return cScalingBit ? ((climbRaw << 1) >> 1) * .5f : ((climbRaw << 1) >> 1) / 10.0f;
        }

        /**
         * @brief Set the climb rate in meters per second.
         * @param climbRate The climb rate in meters per second.
         * @return Reference to the current object.
         */
        TrackingPayload &climbRate(float climbRate)
        {
            int16_t climb10 = etl::clamp(static_cast<int>(roundf(climbRate * 10.0f)), -315, 315);

            if (etl::absolute(climb10) > 63)
            {
                climbRaw = ((climb10 + (climb10 >= 0 ? 2 : -2)) / 5); // set scale factor
                cScalingBit = true;
            }
            else
            {
                climbRaw = climb10;
            }

            return *this;
        }

        /**
         * @brief Get the ground track in degrees.
         * @return The ground track in degrees.
         */
        float groundTrack() const
        {
            return static_cast<float>(groundTrackRaw) * 360.f / 256.f;
        }

        /**
         * @brief Set the ground track in degrees.
         * @param groundTrack The ground track in degrees.
         * @return Reference to the current object.
         */
        TrackingPayload &groundTrack(float groundTrack)
        {
            if (groundTrack < 0.0f)
            {
                groundTrack += 360.0f;
            }
            else if (groundTrack >= 360.0f)
            {
                groundTrack -= 360.0f;
            }

            groundTrackRaw = etl::clamp(static_cast<int>(roundf(groundTrack * 256.0f / 360.0f)), 0, 255);
            return *this;
        }

        /**
         * @brief Check if the turn rate is set.
         * @return True if the turn rate is set, false otherwise.
         */
        bool hasTurnrate() const
        {
            return hasTurnRateRaw;
        }

        /**
         * @brief Get the turn rate in degrees per second.
         * @return The turn rate in degrees per second.
         */
        float turnRate() const
        {
            return tScalingBit ? static_cast<float>(turnRateRaw) : static_cast<float>(turnRateRaw) / 4.0f;
        }

        /**
         * @brief Set the turn rate in degrees per second.
         * @param turnRate The turn rate in degrees per second.
         * @return Reference to the current object.
         */
        TrackingPayload &turnRate(float turnRate)
        {
            hasTurnRateRaw = true;
            int16_t trOs = etl::clamp(static_cast<int>(roundf(turnRate * 4.0f)), -254, 254);
            if (etl::absolute(trOs) >= 63)
            {
                turnRateRaw = ((trOs + (trOs >= 0 ? 2 : -2)) / 4);
                tScalingBit = true;
            }
            else
            {
                turnRateRaw = trOs;
            }

            return *this;
        }

        /**
         * @brief Serialize the tracking payload to a bit stream.
         * @param writer The bit stream writer.
         */
        virtual void serialize(etl::bit_stream_writer &writer) const override
        {
            writer.write_unchecked(etl::reverse_bytes(latitudeRaw << 8), 24U);
            writer.write_unchecked(etl::reverse_bytes(longitudeRaw << 8), 24U);
            writer.write_unchecked(static_cast<uint8_t>(altitudeRaw), 8U);

            writer.write_unchecked(trackingBit);
            writer.write_unchecked(static_cast<uint8_t>(aircraftTypeRaw), 3U);
            writer.write_unchecked(aScaling);
            writer.write_unchecked(static_cast<uint8_t>(altitudeRaw >> 8), 3U);

            writer.write_unchecked(sScalingBit);
            writer.write_unchecked(speedRaw, 7U);

            writer.write_unchecked(cScalingBit);
            writer.write_unchecked(climbRaw, 7U);

            writer.write_unchecked(groundTrackRaw, 8U);

            if (hasTurnRateRaw)
            {
                writer.write_unchecked(tScalingBit);
                writer.write_unchecked(turnRateRaw, 7U);
            }
        }

        /**
         * @brief Deserialize the tracking payload from a bit stream.
         * @param reader The bit stream reader.
         * @return The deserialized tracking payload.
         */
        static const TrackingPayload deserialize(etl::bit_stream_reader &reader)
        {
            TrackingPayload tracking;
            tracking.latitudeRaw = etl::reverse_bytes(reader.read_unchecked<uint32_t>(24U)) >> 8;
            tracking.longitudeRaw = etl::reverse_bytes(reader.read_unchecked<uint32_t>(24U)) >> 8;

            tracking.altitudeRaw = static_cast<uint8_t>(reader.read_unchecked<uint8_t>(8U));

            tracking.trackingBit = reader.read_unchecked<bool>();
            tracking.aircraftTypeRaw = static_cast<AircraftType>(reader.read_unchecked<uint8_t>(3U));
            tracking.aScaling = reader.read_unchecked<bool>();

            tracking.altitudeRaw |= static_cast<uint16_t>(reader.read_unchecked<uint8_t>(3U)) << 8;

            tracking.sScalingBit = reader.read_unchecked<bool>();
            tracking.speedRaw = reader.read_unchecked<uint8_t>(7U);

            tracking.cScalingBit = reader.read_unchecked<bool>();
            tracking.climbRaw = reader.read_unchecked<int8_t>(7U);

            tracking.groundTrackRaw = reader.read_unchecked<uint8_t>(8U);

            auto tScaling = reader.read<bool>();
            if (tScaling)
            {
                tracking.tScalingBit = tScaling.value();
                tracking.turnRateRaw = reader.read_unchecked<int8_t>(7U);
            }

            return tracking;
        }
    };

}