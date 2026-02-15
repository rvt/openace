#pragma once

#include <stdint.h>
#include "etl/bit_stream.h"
#include "etl/vector.h"
#include "address.hpp"

namespace ADSL
{
    /**
     * @brief Represents the header for ADSL protocol.
     */
    class Header final
    {
    public:
        /**
         * @brief Enumeration for message types.
         * ADS-L.4.SRD860.F.2.1
         */
        struct PayloadTypeIdentifier
        {
            enum enum_type : uint8_t
            {
            RESERVED0 = 0,     // Unknown
            RESERVED1 = 1,     // Reserved
            TRAFFIC = 2,       // Traffic
            STATUS = 3,        // Status
            TRAFFICUPLINK = 4, // Traffic Uplink
            FISBUPLINK = 5,    // FISB Uplink
            REMOTEID = 6,      // Remote ID
            };

            ETL_DECLARE_ENUM_TYPE(PayloadTypeIdentifier, uint8_t)
            ETL_ENUM_TYPE(RESERVED0, "Unknown")
            ETL_ENUM_TYPE(RESERVED1, "Reserved")
            ETL_ENUM_TYPE(TRAFFIC, "Traffic")
            ETL_ENUM_TYPE(STATUS, "Status")
            ETL_ENUM_TYPE(TRAFFICUPLINK, "Traffic Uplink")
            ETL_ENUM_TYPE(FISBUPLINK, "FISB Uplink")
            ETL_ENUM_TYPE(REMOTEID, "Remote ID")
            ETL_END_ENUM_TYPE

        };

    private:
        PayloadTypeIdentifier payloadTypeIdentifier_ = PayloadTypeIdentifier::RESERVED1;
        uint8_t addressMappingTable_ = 0x05; // Currently Fixed to ICAO ADS-L.4.SRD860.F.2.2
        Address address_{0};                 // For now we only allows MODE-S ICAO
        bool relay_ = false;

    public:
        /**
         * @brief Constructor with all fields.
         * @param extended_ Extended header bit.
         * @param forward_ Forwarding bit.
         * @param type_ Message type.
         */
        explicit Header() = default;

        Address address() const
        {
            return address_;
        }

        void address(Address adr)
        {
            address_ = adr;
        }
        /**
         * @brief Get the message type.
         * @return The message type.
         */
        PayloadTypeIdentifier type() const
        {
            return payloadTypeIdentifier_;
        }

        /**
         * @brief Set the message type.
         * @param value The message type.
         */
        void type(PayloadTypeIdentifier value)
        {
            payloadTypeIdentifier_ = value;
        }

        /**
         * @brief Get the forwarding bit.
         * @return True if forwarding is enabled, false otherwise.
         */
        bool forward() const
        {
            return relay_;
        }

        /**
         * @brief Set the forwarding bit.
         * @param value True to enable forwarding, false to disable.
         */
        void forward(bool value)
        {
            relay_ = value;
        }

        uint8_t addressMappingTable() const
        {
            return addressMappingTable_;
        }

        void addressMappingTable(uint8_t table)
        {
            addressMappingTable_ = table;
        }

        /**
         * @brief Serialize the header to a bit stream. v1/v2 are the same
         * @param writer The bit stream writer.
         */
        void serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked(static_cast<uint8_t>(payloadTypeIdentifier_), 8U);
            writer.write_unchecked(static_cast<uint8_t>(addressMappingTable_), 6U);
            address_.serialize(writer);
            writer.write_unchecked(0, 1U); 
            writer.write_unchecked(relay_);
        }


        /**
         * @brief Deserialize the header from a bit stream. v1/v2 are the same
         * @param reader The bit stream reader.
         * @return The deserialized header.
         */
        static const Header deserialize(etl::bit_stream_reader &reader)
        {
            Header header;
            header.payloadTypeIdentifier_ =  PayloadTypeIdentifier(reader.read_unchecked<uint8_t>(8));
            header.addressMappingTable_ = reader.read_unchecked<uint8_t>(6);
            header.address_ = Address::deserialize(reader);
            reader.read_unchecked<bool>(); // reserved
            header.relay_ = reader.read_unchecked<bool>();
            return header;

            //             return Header
            // {
            //     PayloadTypeIdentifier(reader.read_unchecked<uint8_t>(8)),
            //         reader.read_unchecked<uint8_t>(6),
            //         Address::deserialize(reader),
            //         reader.read_unchecked<bool>(), // reserved
            //         reader.read_unchecked<bool>()
            // };
        }
    };

}