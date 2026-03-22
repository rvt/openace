#pragma once

#include <stdint.h>
#include "address.hpp"
#include "trafficpayload.hpp"
#include "etl/bit_stream.h"

namespace ADSL
{

    /**
     * @brief One entry in a Traffic Uplink Payload (ADS-L.4.SRD860.G.3).
     *
     * Wire layout per entry (152 bits = 19 bytes):
     *   [6]  addressMappingTable  (per ADS-L.4.SRD860.F.2.2)
     *   [24] address
     *   [2]  reserved (zero)
     *   [120] Traffic Payload     (per ADS-L.4.SRD860.G.1)
     */
    class UplinkEntry
    {
        uint8_t addressMappingTable_ = 0x05; // 6 bits, default: ICAO
        Address address_{};
        TrafficPayload payload_{};

    public:
        // Note 191 bytes, because (191-3) = 188 which can be devided by 4
        // Maxumum size of a UplinkEntry = 1 + 5 + 191 + 3 = 199 byte
        static constexpr size_t LENGTH = 1 + Address::LENGTH + TrafficPayload::LENGTH; // +1 is for addressMappingTable_ + 2 reserve

        explicit UplinkEntry() = default;
        UplinkEntry(uint8_t addressMappingTable, Address address, const TrafficPayload &payload)
            : addressMappingTable_(addressMappingTable), address_(address), payload_(payload) {}

        UplinkEntry(uint8_t addressMappingTable, uint32_t address, const TrafficPayload &payload)
            : addressMappingTable_(addressMappingTable), address_(Address(address)), payload_(payload) {}

        uint8_t addressMappingTable() const { return addressMappingTable_; }
        void addressMappingTable(uint8_t value) { addressMappingTable_ = value; }

        Address address() const { return address_; }
        void address(Address value) { address_ = value; }

        const TrafficPayload &payload() const { return payload_; }
        void payload(const TrafficPayload &value) { payload_ = value; }


        /** 
         * Calculate the total size of the payload including padding
         */
        static size_t calculateSizeWithPadding(size_t entries) {
            auto sizeBytes = (entries * LENGTH);
            const size_t padBytes = (3 - sizeBytes) & 0x3;
            return sizeBytes + padBytes;
        }

        /**
         * @brief Serialize this entry into the bit stream.
         * @return number of bytes written (always 19)
         */
        size_t serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked(addressMappingTable_, 6U);
            address_.serialize(writer);
            writer.write_unchecked(0U, 2U); // reserved
            payload_.serialize_issue1(writer);
            return LENGTH;
        }

        /**
         * @brief Deserialize one entry from the bit stream.
         */
        static UplinkEntry deserialize(etl::bit_stream_reader &reader)
        {
            UplinkEntry entry;
            entry.addressMappingTable_ = reader.read_unchecked<uint8_t>(6U);
            entry.address_ = Address::deserialize(reader);
            reader.read_unchecked<uint8_t>(2U); // reserved
            entry.payload_ = TrafficPayload::deserialize_issue1(reader);
            return entry;
        }
    };

} // namespace ADSL
