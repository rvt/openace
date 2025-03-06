#pragma once

#include <stdint.h>
#include "etl/math.h"
#include "header.hpp"

namespace FANET
{
    /**
     * @brief Ground Tracking Payload for FANET protocol.
     * Messagetype : 7
     */
    class GroundTrackingPayload final
    {
    public:
        /**
         * @brief Enumeration for tracking types.
         */
        enum class TrackingType : uint8_t
        {
            OTHER = 0,                   // Other
            WALKING = 1,                 // Walking
            VEHICLE = 2,                 // Vehicle
            BIKE = 3,                    // Bike
            BOOT = 4,                    // Boot
            NEED_A_RIDE = 8,             // Need a ride
            NEED_TECHNICAL_SUPPORT = 12, // Need technical support
            NEED_MEDICAL_HELP = 13,      // Need medical help
            DISTRESS_CALL = 14,          // Distress call
            DISTRESS_CALL_AUTO = 15      // Distress call (automatic)
        };

    private:
        int32_t latitudeRaw = 0;  // Scaled by 93206
        int32_t longitudeRaw = 0; // Scaled by 46603
        TrackingType groundTypeRaw = TrackingType::OTHER;
        uint8_t unkRaw = 0;
        bool trackingBit = false;

    public:
        /**
         * @brief Default constructor.
         */
        explicit GroundTrackingPayload() = default;

        /**
         * @brief Constructor with all fields.
         * @param latitudeRaw_ Raw latitude value.
         * @param longitudeRaw_ Raw longitude value.
         * @param groundTypeRaw_ Ground tracking type.
         * @param unkRaw_ Unknown raw value.
         * @param trackingBit_ Tracking bit.
         */
        GroundTrackingPayload(uint32_t latitudeRaw_, uint32_t longitudeRaw_,
                              TrackingType groundTypeRaw_, uint8_t unkRaw_, bool trackingBit_)
            : latitudeRaw(latitudeRaw_),
              longitudeRaw(longitudeRaw_),
              groundTypeRaw(groundTypeRaw_),
              unkRaw(unkRaw_),
              trackingBit(trackingBit_)
        {
        }

        /**
         * @brief Get the message type.
         * @return The message type.
         */
        Header::MessageType type() const
        {
            return Header::MessageType::GROUND_TRACKING;
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
        GroundTrackingPayload &latitude(float lat)
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
        GroundTrackingPayload &longitude(float lon)
        {
            lon = etl::clamp(lon, -180.f, 180.f);
            longitudeRaw = roundf(lon * 46603.0f);
            return *this;
        }

        /**
         * @brief Get if this aircraft allows tracking.
         * @return True if tracking is allowed, false otherwise.
         */
        bool tracking() const
        {
            return trackingBit;
        }

        /**
         * @brief Set if this aircraft allows tracking.
         * @param tracking_ True if tracking is allowed, false otherwise.
         * @return Reference to the current object.
         */
        GroundTrackingPayload &tracking(bool tracking_)
        {
            trackingBit = tracking_;
            return *this;
        }

        /**
         * @brief Get the type of ground tracking.
         * @return The type of ground tracking.
         */
        TrackingType groundType() const
        {
            return groundTypeRaw;
        }

        /**
         * @brief Set the type of ground tracking.
         * @param groundType The type of ground tracking.
         * @return Reference to the current object.
         */
        GroundTrackingPayload &groundType(TrackingType groundType)
        {
            groundTypeRaw = groundType;
            return *this;
        }

        /**
         * @brief Serialize the ground tracking payload to a bit stream.
         * @param writer The bit stream writer.
         */
        void serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked(etl::reverse_bytes(latitudeRaw << 8), 24U);
            writer.write_unchecked(etl::reverse_bytes(longitudeRaw << 8), 24U);
            writer.write_unchecked(static_cast<uint8_t>(groundTypeRaw), 4U);
            writer.write_unchecked(unkRaw, 3U);
            writer.write_unchecked(trackingBit);
        }

        /**
         * @brief Deserialize the ground tracking payload from a bit stream.
         * @param reader The bit stream reader.
         * @return The deserialized ground tracking payload.
         */
        static const GroundTrackingPayload deserialize(etl::bit_stream_reader &reader)
        {
            GroundTrackingPayload payload;
            payload.latitudeRaw = etl::reverse_bytes(reader.read_unchecked<uint32_t>(24U)) >> 8;
            payload.longitudeRaw = etl::reverse_bytes(reader.read_unchecked<uint32_t>(24U)) >> 8;
            payload.groundTypeRaw = static_cast<TrackingType>(reader.read_unchecked<uint8_t>(4U));
            payload.unkRaw = reader.read_unchecked<uint8_t>(3U);
            payload.trackingBit = reader.read_unchecked<bool>();
            return payload;
        }
    };
}
