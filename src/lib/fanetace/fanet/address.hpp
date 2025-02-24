#pragma once

#include <stdint.h>

namespace FANET
{

    // FANET Address Structure
    class Address final
    {
    private:
        uint8_t mfgId= 0;     // Manufacturer ID
        uint16_t uniqueId = 0; // Unique Device ID
    public:
        Address() = default;

        // Constructor 1: Takes a uint32_t and splits it into manufacturerId and uniqueID
        Address(uint32_t combinedId) : Address(mfgId = static_cast<uint8_t>((combinedId >> 16) & 0xFF), uniqueId = static_cast<uint16_t>(combinedId & 0xFFFF))
        {
        }

        Address(uint8_t manufacturerId_, uint16_t uniqueId_) : mfgId(manufacturerId_), uniqueId(uniqueId_)
        {
        }

        uint8_t manufacturer() const
        {
            return mfgId;
        }
        void manufacturer(uint8_t value)
        {
            mfgId = value;
        }

        uint16_t unique() const
        {
            return uniqueId;
        }
        void unique(uint16_t value)
        {
            uniqueId = value;
        }

        uint32_t combined() const
        {
            return (static_cast<uint32_t>(mfgId) << 16) | uniqueId;
        }

        void serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked<uint8_t>(mfgId);
            writer.write_unchecked<uint16_t>(etl::reverse_bytes<uint16_t>(uniqueId));
        }

        static const Address deserialize(etl::bit_stream_reader &reader)
        {
            Address address;
            address.mfgId = reader.read_unchecked<uint8_t>();
            address.uniqueId = etl::reverse_bytes<uint16_t>(reader.read_unchecked<uint16_t>());
            return address;
        }
    };
}
