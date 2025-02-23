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

    public:
        int32_t latitudeRaw : 24;  // Scaled by 93206
        int32_t longitudeRaw : 24; // Scaled by 46603
        uint16_t altitudeRaw : 11; // 11-bit altitude
        bool aScaling : 1;
        AircraftType aircraftTypeRaw : 3;
        bool trackingBit : 1;
        uint8_t speedRaw : 7;
        bool sScalingBit : 1;
        int8_t climbRaw : 7;
        bool cScalingBit : 1;
        uint8_t groundTrackRaw : 8;
        uint8_t turnRateRaw : 7;
        bool tScalingBit : 1;

    public:
        TrackingPayload()
            : latitudeRaw(0),
              longitudeRaw(0),
              altitudeRaw(0),
              aScaling(false),
              aircraftTypeRaw(AircraftType::OTHER),
              trackingBit(false),
              speedRaw(0),
              sScalingBit(false),
              climbRaw(0),
              cScalingBit(false),
              groundTrackRaw(0),
              turnRateRaw(0),
              tScalingBit(false)
        {
        }

        Header::MessageType type() const
        {
            return Header::MessageType::TRACKING;
        }

        float latitude() const
        {
            return ((latitudeRaw << 8) >> 8) / 93206.f;
        }

        float longitude() const
        {
            return ((longitudeRaw << 8) >> 8) / 46603.f;
        }

        TrackingPayload &latitude(float lat)
        {
            lat = etl::clamp(lat, -90.f, 90.f);
            latitudeRaw = roundf(lat * 93206.0f);
            return *this;
        }

        TrackingPayload &longitude(float lon)
        {
            lon = etl::clamp(lon, -180.f, 180.f);
            longitudeRaw = roundf(lon * 46603.0f);
            return *this;
        }

        /**
         * Get Altitude in Meters
         */
        int16_t altitude() const
        {
            return aScaling ? altitudeRaw << 2 : altitudeRaw;
        }

        /**
         * Set Altitude in Meters
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
         * Get if this aircraft allows tracking
         */
        bool tracking() const
        {
            return trackingBit;
        }

        /**
         * Set if this aircraft allows tracking
         */
        TrackingPayload &tracking(bool tracking)
        {
            trackingBit = tracking;
            return *this;
        }

        /**
         * Get the type of aircraft
         */
        AircraftType aircraftType() const
        {
            return aircraftTypeRaw;
        }

        /**
         * Set the type of aircraft
         */
        TrackingPayload &aircraftType(AircraftType aircraftType)
        {
            aircraftTypeRaw = aircraftType;
            return *this;
        }

        /**
         * Get the speed in kilometer per hour
         */
        float speed() const
        {
            return sScalingBit ? speedRaw * 2.5f : speedRaw / 2.f;
        }

        /**
         * Set the speed in kilometer per hour
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
         * get the climbrate in m/s

         */
        float climbRate() const
        {
            return cScalingBit ? ((climbRaw << 1) >> 1) * .5f : ((climbRaw << 1) >> 1) / 10.0f;
        }

        /**
         * Set the climbrate in m/s
         */
        TrackingPayload &climbRate(float climbRate)
        {
            int16_t climb10 = etl::clamp((int)roundf(climbRate * 10.0f), -315, 315);

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
         * Get the heading in degrees
         */
        float groundTrack() const
        {
            return (float)groundTrackRaw * 360.f / 256.f;
        }

        /**
         * Set the groundtrack
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

            groundTrackRaw = etl::clamp(((int)roundf(groundTrack * 256.0f / 360.0f)), 0, 255);
            return *this;
        }

        /**
         * Get the turnrate in degrees per second
         */
        float turnRate() const
        {
            if (tScalingBit)
            {
                return turnRateRaw * 4.0f;
            }
            else
            {
                return turnRateRaw;
            }
        }

        /**
         * Set the turnrate im degrees per second
         */
        TrackingPayload &turnRate(float turnRate)
        {
            int16_t trOs = etl::clamp(static_cast<int>(roundf(turnRate * 4.0f)), -254, 254);
            if (etl::absolute(trOs) >= 63)
            {
                turnRateRaw = ((trOs + (trOs >= 0 ? 2 : -2)) / 4);
                tScalingBit = true;
            }
            else
            {
                turnRateRaw = roundf(turnRate);
            }

            return *this;
        }

        virtual void serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked(etl::reverse_bytes(latitudeRaw >> 8), 24U);
            writer.write_unchecked(etl::reverse_bytes(longitudeRaw >> 8), 24U);
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

            // TODO: make turnrate optional
            writer.write_unchecked(tScalingBit);
            writer.write_unchecked(turnRateRaw, 7U);
        }

        virtual void deserialize(etl::bit_stream_reader &reader)
        {
            latitudeRaw = etl::reverse_bytes(reader.read_unchecked<uint32_t>(24U)) << 8;
            longitudeRaw = etl::reverse_bytes(reader.read_unchecked<uint32_t>(24U)) << 8;

            altitudeRaw = static_cast<uint8_t>(reader.read_unchecked<uint8_t>(8U));

            trackingBit = reader.read_unchecked<bool>();
            aircraftTypeRaw = static_cast<AircraftType>(reader.read_unchecked<uint8_t>(3U));
            aScaling = reader.read_unchecked<uint8_t>();

            altitudeRaw |= static_cast<uint16_t>(reader.read_unchecked<uint8_t>(3U)) << 8;

            sScalingBit = reader.read_unchecked<bool>();
            speedRaw = reader.read_unchecked<uint8_t>(7U);

            cScalingBit = reader.read_unchecked<bool>();
            climbRaw = reader.read_unchecked<uint8_t>(7U);

            groundTrackRaw = reader.read_unchecked<uint8_t>(8U);

            // TODO: make turnrate optional
            tScalingBit = reader.read_unchecked<bool>();
            turnRateRaw = reader.read_unchecked<uint8_t>(7U);
        }

    } __attribute__((packed));

}