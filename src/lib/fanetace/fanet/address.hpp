#pragma once

#include <stdint.h>

namespace FANET
{

    // FANET Address Structure
    class Address final
    {
    private:
        uint8_t mfgId;     // Manufacturer ID
        uint16_t uniqueId; // Unique Device ID
    public:
        // Constructor 1: Takes a uint32_t and splits it into manufacturerId and uniqueID
        explicit Address(uint32_t combinedId)
        {
            mfgId = static_cast<uint8_t>((combinedId >> 16) & 0xFF);
            uniqueId = static_cast<uint16_t>(combinedId & 0xFFFF);
        }
        Address(uint8_t manufacturerId_, uint16_t uniqueId_) : mfgId(manufacturerId_), uniqueId(uniqueId_)
        {
        }

        // Default constructor (if needed)
        Address() = default;

        // Methods without `get` and `set` prefixes
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

        void deserialize(etl::bit_stream_reader &reader)
        {
            mfgId = reader.read_unchecked<uint8_t>();
            uniqueId = etl::reverse_bytes<uint16_t>(reader.read_unchecked<uint16_t>());
        }

    } __attribute__((packed));

}
