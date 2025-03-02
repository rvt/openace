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
            SINGLEHOP = 1,
            TWOHOP = 2, // via forward, if received via forward (received forward bit = 0). must be used if forward is set
            RESERVED = 3
        };

    private:
        AckType ackType = AckType::NONE; // Acknowledgment
        bool isUnicast = false;          // 1: Unicast or 0:Broadcast
        bool hasSignature = false;       // Signature presence
        uint8_t reservedBits = 0;        // Reserved bits
        bool isGeoForward = false;         // Forwarding bit
    public:
        explicit ExtendedHeader() = default;

        ExtendedHeader(AckType ackType_, bool isUnicast_, bool hasSignature_, bool isGeoForward_)
        : ackType(ackType_),
          isUnicast(isUnicast_),
          hasSignature(hasSignature_),
          reservedBits(0),
          isGeoForward(isGeoForward_) {}

        bool geoForward() const
        {
            return isGeoForward;
        }
        void geoForward(bool value)
        {
            isGeoForward = value;
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
            writer.write_unchecked(0, 3U);
            writer.write_unchecked(isGeoForward);
        }

        static const ExtendedHeader deserialize(etl::bit_stream_reader &reader)
        {
            ExtendedHeader eHeader;
            eHeader.ackType = static_cast<AckType>(reader.read_unchecked<uint8_t>(2U));
            eHeader.isUnicast = reader.read_unchecked<bool>();
            eHeader.hasSignature = reader.read_unchecked<bool>();
            eHeader.reservedBits = reader.read_unchecked<uint8_t>(3U);
            eHeader.isGeoForward = reader.read_unchecked<bool>();
            return eHeader;
        }
    };
}