#pragma once

#include <stdint.h>
#include "etl/string.h"
#include "etl/vector.h"
#include "etl/bit_stream.h"
#include "math.h"

// FANET Header Structure
namespace FANET
{
#define CONSTRAIN(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

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

    enum class MessageType : uint8_t
    {
        ACK = 0,
        TRACKING = 1,
        NAME = 2,
        MESSAGE = 3,
        SERVICE = 4,
        LANDMARKS = 5,
        REMOTE_CONFIG = 6,
        GROUND_TRACKING = 7
    };

    enum class GroundTrackingType : uint8_t
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

    enum class AckType : uint8_t
    {
        NONE = 0,
        REQUEST = 1,
        REQUEST_VIA_FORWARD = 2, // via forward, if received via forward (received forward bit = 0). must be used if forward is set
        RESERVED = 3
    };

    class Header
    {
    public:
        MessageType type : 6; // Type of message (6 bits)
        bool forward : 1;     // Forwarding bit
        bool extended : 1;    // Extended header bit
    } __attribute__((packed));

    // FANET Address Structure
    class Address
    {
        uint8_t manufacturerId; // Manufacturer ID
        uint16_t uniqueID;      // Unique Device ID
    public:
        // Constructor 1: Takes a uint32_t and splits it into manufacturerId and uniqueID
        explicit Address(uint32_t combinedId)
        {
            manufacturerId = static_cast<uint8_t>((combinedId >> 16) & 0xFF);
            uniqueID = static_cast<uint16_t>(combinedId & 0xFFFF);
        }
        Address(uint8_t manufacturerId_, uint16_t uniqueID_) : manufacturerId(manufacturerId_), uniqueID(uniqueID_)
        {
        }

        // Default constructor (if needed)
        Address() = default;
    } __attribute__((packed));

    // FANET Extended Header
    class ExtendedHeader
    {
    public:
        uint8_t reserved : 3;  // Reserved bits
        bool geoForwarded : 1; // Forwarding bit
        bool signature : 1;    // Signature presence
        bool cast : 1;         // 1: Unicast or 0:Broadcast
        AckType ack : 2;       // Acknowledgment
    } __attribute__((packed));

    class AckPayload
    {
    public:
        MessageType type() const
        {
            return MessageType::ACK;
        }
    };

    /**
     * Tracking payload
     * Messagetype : 1
     */
    class TrackingPayload
    {
    private:
        int32_t latitudeRaw : 24;  // Scaled by 93206
        int32_t longitudeRaw : 24; // Scaled by 46603
        uint16_t altitudeRaw : 11; // 11-bit altitude
        bool aScaling : 1;
        AircraftType aircraftTypeRaw : 3;
        bool trackingBit : 1;
        uint8_t speedRaw : 7;
        bool sScalingBit : 1;
        int8_t climbRaw : 7;
        bool cScaling : 1;
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
              cScaling(false),
              groundTrackRaw(0),
              turnRateRaw(0),
              tScalingBit(false)
        {
        }

        MessageType type() const
        {
            return MessageType::TRACKING;
        }

        float latitude()
        {
            return ((latitudeRaw << 8) >> 8) / 93206.f;
        }
        float longitude()
        {
            return ((longitudeRaw << 8) >> 8) / 46603.f;
        }
        TrackingPayload &latitude(float lat)
        {
            lat = CONSTRAIN(lat, -90.f, 90.f);
            latitudeRaw = roundf(lat * 93206.0f);
            return *this;
        }
        TrackingPayload &longitude(float lon)
        {
            lon = CONSTRAIN(lon, -180.f, 180.f);
            longitudeRaw = roundf(lon * 46603.0f);
            return *this;
        }

        /**
         * Get Altitude in Meters
         */
        int16_t altitude()
        {
            return aScaling ? altitudeRaw << 2 : altitudeRaw;
        }

        /**
         * Set Altitude in Meters
         */
        TrackingPayload &altitude(int16_t alt)
        {
            alt = CONSTRAIN(alt, 0, 8188);
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
        bool tracking()
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
        AircraftType aircraftType()
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
        float speed()
        {
            return sScalingBit ? speedRaw * 2.5f : speedRaw / 2.f;
        }

        /**
         * Set the speed in kilometer per hour
         */
        TrackingPayload &speed(float speed)
        {
            int32_t speed2 = CONSTRAIN((int32_t)roundf(speed * 2.0f), 0, 127 * 5);
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
        float climbRate()
        {
            return cScaling ? ((climbRaw << 1) >> 1) * .5f : ((climbRaw << 1) >> 1) / 10.0f;
        }

        /**
         * Set the climbrate in m/s
         */
        TrackingPayload &climbRate(float climbRate)
        {
            int16_t climb10 = CONSTRAIN((int)roundf(climbRate * 10.0f), -315, 315);

            if (abs(climb10) > 63)
            {
                climbRaw = ((climb10 + (climb10 >= 0 ? 2 : -2)) / 5); // set scale factor
                cScaling = true;
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
        float groundTrack()
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

            groundTrackRaw = CONSTRAIN(((int)roundf(groundTrack * 256.0f / 360.0f)), 0, 255);
            return *this;
        }

        /**
         * Get the turnrate in degrees per second
         */
        float turnRate()
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
            int16_t trOs = CONSTRAIN((int16_t)roundf(turnRate * 4.0f), -254, 254);
            if (abs(trOs) >= 63)
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

    } __attribute__((packed));

    /**
     * Name Payload
     * Messagetype : 2
     */
    template <size_t SIZE>
    class NamePayload
    {
        etl::string<SIZE> nameRaw;
        static_assert(SIZE <= 245, "NamePayload size cannot exceed 245 bytes");

    public:
        MessageType type() const
        {
            return MessageType::NAME;
        }

        etl::string_view name() const
        {
            return etl::string_view(nameRaw);
        }

        void name(const etl::string_view &name)
        {
            nameRaw.assign(name.data(), name.size());
        }
    };

    /**
     * Message Payload
     * Messagetype : 3
     */
    template <size_t SIZE>
    class MessagePayload
    {
        uint8_t subHeaderRaw;
        etl::vector<uint8_t, SIZE> messageRaw;
        static_assert(SIZE <= 244, "MessagePayload size cannot exceed 244 bytes");

    public:
        MessagePayload() : subHeaderRaw(0) {}
        MessageType type() const
        {
            return MessageType::MESSAGE;
        }
        void subHeader(uint8_t subHeader)
        {
            subHeaderRaw = subHeader;
        }
        uint8_t subHeader()
        {
            return subHeaderRaw;
        }
        const etl::ivector<uint8_t> &message() const
        {
            return messageRaw;
        }

        void message(const etl::ivector<uint8_t> &message)
        {
            size_t copy_size = std::min(message.size(), messageRaw.capacity());
            messageRaw.assign(message.begin(), message.begin() + copy_size);
        }
    };

    /**
     * Ground Tracking Payload
     * Messagetype : 7
     */
    class GroundTrackingPayload
    {
        uint32_t latitudeRaw : 24;  // Scaled by 93206
        uint32_t longitudeRaw : 24; // Scaled by 46603
        bool trackingBit : 1;
        uint8_t unkRaw : 3;
        GroundTrackingType groundTypeRaw : 4;

    public:
        GroundTrackingPayload()
            : latitudeRaw(0),
              longitudeRaw(0),
              trackingBit(false),
              unkRaw(0),
              groundTypeRaw(GroundTrackingType::OTHER)
        {
        }
        MessageType type() const
        {
            return MessageType::GROUND_TRACKING;
        }

        float latitude()
        {
            return ((latitudeRaw << 8) >> 8) / 93206.f;
        }
        float longitude()
        {
            return ((longitudeRaw << 8) >> 8) / 46603.f;
        }
        uint8_t unk()
        {
            return unkRaw;
        }
        GroundTrackingPayload &latitude(float lat)
        {
            lat = CONSTRAIN(lat, -90.f, 90.f);
            latitudeRaw = roundf(lat * 93206.0f);
            return *this;
        }
        GroundTrackingPayload &longitude(float lon)
        {
            lon = CONSTRAIN(lon, -180.f, 180.f);
            longitudeRaw = roundf(lon * 46603.0f);
            return *this;
        }
        /**
         * Get if this aircraft allows tracking
         */
        bool tracking()
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
        GroundTrackingType groundType()
        {
            return groundTypeRaw;
        }

        /**
         * Set the type of aircraft
         */
        GroundTrackingPayload &groundType(GroundTrackingType groundType)
        {
            groundTypeRaw = groundType;
            return *this;
        }

    } __attribute__((packed));

    /**
     * Service Payload
     * Messagetype : 4
     */
    // class ServicePayload
    // {
    // public:
    //     MessageType type() const
    //     {
    //         return MessageType::SERVICE;
    //     }

    //     bool exAvailable : 1;
    //     uint8_t tbd : 2;
    //     bool baromAvailable : 1;
    //     bool humidAvailable : 1;
    //     bool windAvailable : 1;
    //     bool tempAvailable : 1;
    //     bool gateway : 1;
    //     uint8_t extendedHeader : 8;
    //     uint32_t latitude : 24;  // Scaled by 93206
    //     uint32_t longitude : 24; // Scaled by 46603
    //     uint8_t temperature : 8; // ±0.5°C per bit
    //     uint8_t windHeading : 8;
    //     uint8_t windSpeed : 7;
    //     uint8_t sScale : 1;
    //     uint8_t windGusts : 7;
    //     uint8_t gScale : 1;
    //     uint8_t humidity : 8;     // 0.4 %rh per bit
    //     uint16_t barometric : 16; // 0.01 hPa per bit, offset 430 hPa
    // } __attribute__((packed));

};
