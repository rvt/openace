#pragma once

#include <stdint.h>

namespace FANET
{

    class ExtendedHeader final
    {
    public:
        enum class AckType : uint8_t
        {
            NONE = 0,
            REQUEST = 1,
            REQUEST_VIA_FORWARD = 2, // via forward, if received via forward (received forward bit = 0). must be used if forward is set
            RESERVED = 3
        };

    private:
        uint8_t reservedBits : 3; // Reserved bits
        bool geoForward : 1;      // Forwarding bit
        bool hasSignature : 1;    // Signature presence
        bool isUnicast : 1;       // 1: Unicast or 0:Broadcast
        AckType ackType : 2;      // Acknowledgment
    public:
        ExtendedHeader() : reservedBits(0), geoForward(false), hasSignature(false), isUnicast(false), ackType(AckType::NONE) {}

        ExtendedHeader(uint8_t reservedBits, bool geoForward, bool hasSignature, bool isUnicast, AckType ackType)
            : reservedBits(reservedBits), geoForward(geoForward), hasSignature(hasSignature), isUnicast(isUnicast), ackType(ackType) {}

        bool geo_forward() const
        {
            return geoForward;
        }
        void geo_forward(bool value)
        {
            geoForward = value;
        }

        bool signature() const
        {
            return hasSignature;
        }
        void signature(bool value)
        {
            hasSignature = value;
        }

        bool unicast() const
        {
            return isUnicast;
        }
        void unicast(bool value)
        {
            isUnicast = value;
        }

        AckType ack() const
        {
            return ackType;
        }
        void ack(AckType value)
        {
            ackType = value;
        }

        void serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked(static_cast<uint8_t>(ackType), 2U);
            writer.write_unchecked(isUnicast);
            writer.write_unchecked(hasSignature);
            writer.write_unchecked(0, 4U);
        }

        void deserialize(etl::bit_stream_reader &reader)
        {
            ackType = static_cast<AckType>(reader.read_unchecked<uint8_t>(2U));
            isUnicast = reader.read_unchecked<bool>();
            hasSignature = reader.read_unchecked<bool>();
            reader.read_unchecked<uint8_t>(4U); // Ignoring the 4 bits as they're set to 0 during serialization
        }

    } __attribute__((packed));

}