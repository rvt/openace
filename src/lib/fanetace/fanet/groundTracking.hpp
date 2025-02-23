#pragma once

#include <stdint.h>
// #include "etl/string.h"
// #include "etl/vector.h"
// #include "etl/bit_stream.h"
// #include "etl/algorithm.h"
#include "etl/math.h"
#include "header.hpp"

namespace FANET
{

    /**
     * Ground Tracking Payload
     * Messagetype : 7
     */
    class GroundTrackingPayload final
    {
    public:
        enum class TrackingType : uint8_t
        {
            OTHER = 0,
            WALKING = 1,
            VEHICLE = 2,
            BIKE = 3,
            BOOT = 4,
            NEED_A_RIDE = 8,
            NEED_TECHNICAL_SUPPORT = 12,
            NEED_MEDICAL_HELP = 13,
            DISTRESS_CALL = 14,
            DISTRESS_CALL_AUTO = 15
        };

    private:
        uint32_t latitudeRaw : 24;  // Scaled by 93206
        uint32_t longitudeRaw : 24; // Scaled by 46603
        bool trackingBit : 1;
        uint8_t unkRaw : 3;
        TrackingType groundTypeRaw : 4;

    public:
        GroundTrackingPayload()
            : latitudeRaw(0),
              longitudeRaw(0),
              trackingBit(false),
              unkRaw(0),
              groundTypeRaw(TrackingType::OTHER)
        {
        }
        Header::MessageType type() const
        {
            return Header::MessageType::GROUND_TRACKING;
        }

        float latitude() const
        {
            return ((latitudeRaw << 8) >> 8) / 93206.f;
        }
        float longitude() const
        {
            return ((longitudeRaw << 8) >> 8) / 46603.f;
        }
        uint8_t unk() const
        {
            return unkRaw;
        }
        GroundTrackingPayload &latitude(float lat)
        {
            lat = etl::clamp(lat, -90.f, 90.f);
            latitudeRaw = roundf(lat * 93206.0f);
            return *this;
        }
        GroundTrackingPayload &longitude(float lon)
        {
            lon = etl::clamp(lon, -180.f, 180.f);
            longitudeRaw = roundf(lon * 46603.0f);
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
        GroundTrackingPayload &tracking(bool tracking_)
        {
            trackingBit = tracking_;
            return *this;
        }
        /**
         * Get the type of aircraft
         */
        TrackingType groundType() const
        {
            return groundTypeRaw;
        }

        /**
         * Set the type of aircraft
         */
        GroundTrackingPayload &groundType(TrackingType groundType)
        {
            groundTypeRaw = groundType;
            return *this;
        }

        virtual void serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked(etl::reverse_bytes(latitudeRaw >> 8), 24U);
            writer.write_unchecked(etl::reverse_bytes(longitudeRaw >> 8), 24U);

            writer.write_unchecked(static_cast<uint8_t>(groundTypeRaw), 4U);
            writer.write_unchecked(unkRaw, 3U);
            writer.write_unchecked(trackingBit);
        }

        virtual void deserialize(etl::bit_stream_reader &reader)
        {
            latitudeRaw = etl::reverse_bytes(reader.read_unchecked<uint32_t>(24U)) << 8;
            longitudeRaw = etl::reverse_bytes(reader.read_unchecked<uint32_t>(24U)) << 8;

            groundTypeRaw = static_cast<TrackingType>(reader.read_unchecked<uint8_t>(4U));
            unkRaw = reader.read_unchecked<uint8_t>(3U);
            trackingBit = reader.read_unchecked<bool>();
        }

    } __attribute__((packed));
}
