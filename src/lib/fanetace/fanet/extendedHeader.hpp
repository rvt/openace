#pragma once

#include <stdint.h>
#include "etl/bit_stream.h"

namespace FANET
{
    /**
     * @brief Represents the extended header for FANET protocol.
     */
    class ExtendedHeader final
    {
    public:
        /**
         * @brief Enumeration for acknowledgment types.
         */
        enum class AckType : uint8_t
        {
            NONE = 0,       // No acknowledgment
            SINGLEHOP = 1,  // Single-hop acknowledgment
            TWOHOP = 2,     // Two-hop acknowledgment (via forward)
            RESERVED = 3    // Reserved
        };

    private:
        AckType ackType = AckType::NONE; // Acknowledgment type
        bool isUnicast = false;          // Unicast (true) or Broadcast (false)
        bool hasSignature = false;       // Signature presence
        uint8_t reservedBits = 0;        // Reserved bits
        bool isGeoForward = false;       // Forwarding bit

    public:
        /**
         * @brief Default constructor.
         */
        explicit ExtendedHeader() = default;

        /**
         * @brief Constructor with all fields.
         * @param ackType_ Acknowledgment type.
         * @param isUnicast_ Unicast (true) or Broadcast (false).
         * @param hasSignature_ Signature presence.
         * @param isGeoForward_ Forwarding bit.
         */
        ExtendedHeader(AckType ackType_, bool isUnicast_, bool hasSignature_, bool isGeoForward_)
            : ackType(ackType_),
              isUnicast(isUnicast_),
              hasSignature(hasSignature_),
              reservedBits(0),
              isGeoForward(isGeoForward_) {}

        /**
         * @brief Get the forwarding bit.
         * @return True if forwarding is enabled, false otherwise.
         */
        bool geoForward() const
        {
            return isGeoForward;
        }

        /**
         * @brief Set the forwarding bit.
         * @param value True to enable forwarding, false to disable.
         */
        void geoForward(bool value)
        {
            isGeoForward = value;
        }

        /**
         * @brief Get the signature presence.
         * @return True if signature is present, false otherwise.
         */
        bool signature() const
        {
            return hasSignature;
        }

        /**
         * @brief Set the signature presence.
         * @param value True to indicate signature presence, false to indicate absence.
         */
        void signature(bool value)
        {
            hasSignature = value;
        }

        /**
         * @brief Get the unicast/broadcast status.
         * @return True if unicast, false if broadcast.
         */
        bool unicast() const
        {
            return isUnicast;
        }

        /**
         * @brief Set the unicast/broadcast status.
         * @param value True for unicast, false for broadcast.
         */
        void unicast(bool value)
        {
            isUnicast = value;
        }

        /**
         * @brief Get the acknowledgment type.
         * @return The acknowledgment type.
         */
        AckType ack() const
        {
            return ackType;
        }

        /**
         * @brief Set the acknowledgment type.
         * @param value The acknowledgment type.
         */
        void ack(AckType value)
        {
            ackType = value;
        }

        /**
         * @brief Serialize the extended header to a bit stream.
         * @param writer The bit stream writer.
         */
        void serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked(static_cast<uint8_t>(ackType), 2U);
            writer.write_unchecked(isUnicast);
            writer.write_unchecked(hasSignature);
            writer.write_unchecked(0, 3U);
            writer.write_unchecked(isGeoForward);
        }

        /**
         * @brief Deserialize the extended header from a bit stream.
         * @param reader The bit stream reader.
         * @return The deserialized extended header.
         */
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

        void print() const
        {
            printf("ExtendedHeader [AckType: %d (%s), Unicast: %s, Signature: %s, GeoForward: %s] ",
                static_cast<int>(ackType),
                ackType == AckType::NONE ? "NONE     " : ackType == AckType::SINGLEHOP ? "SINGLEHOP"
                                                : ackType == AckType::TWOHOP      ? "TWOHOP   "
                                                                                    : "RESERVED ",
                isUnicast ? "Yes" : "No ",
                hasSignature ? "Yes" : "No ",
                isGeoForward ? "Yes" : "No ");
        }
    };

    
}