#pragma once

#include <stdint.h>
#include "etl/bit_stream.h"
#include "etl/binary.h"

namespace FANET
{
    /**
     * @brief FANET Address Structure
     */
    class Address final
    {
    private:
        uint8_t mfgId = 0;     ///< Manufacturer ID
        uint16_t uniqueId = 0; ///< Unique Device ID

    public:
        /**
         * @brief Default constructor.
         */
        constexpr Address() = default;

        /**
         * @brief Constructor that takes a uint32_t and splits it into manufacturerId and uniqueID.
         * @param asUintId The combined manufacturer and unique ID.
         */
        constexpr explicit Address(uint32_t asUintId)
            : Address(static_cast<uint8_t>((asUintId >> 16) & 0xFF), static_cast<uint16_t>(asUintId & 0xFFFF))
        {
        }

        /**
         * @brief Constructor that takes manufacturerId and uniqueId.
         * @param manufacturerId_ The manufacturer ID.
         * @param uniqueId_ The unique device ID.
         */
        constexpr Address(uint8_t manufacturerId_, uint16_t uniqueId_)
            : mfgId(manufacturerId_), uniqueId(uniqueId_)
        {
        }

        /**
         * @brief Get the manufacturer ID.
         * @return The manufacturer ID.
         */
        constexpr uint8_t manufacturer() const
        {
            return mfgId;
        }

        /**
         * @brief Set the manufacturer ID.
         * @param value The manufacturer ID.
         */
        void manufacturer(uint8_t value)
        {
            mfgId = value;
        }

        /**
         * @brief Get the unique device ID.
         * @return The unique device ID.
         */
        constexpr uint16_t unique() const
        {
            return uniqueId;
        }

        /**
         * @brief Set the unique device ID.
         * @param value The unique device ID.
         */
        void unique(uint16_t value)
        {
            uniqueId = value;
        }

        /**
         * @brief Get the combined manufacturer and unique ID as a uint32_t.
         * @return The combined manufacturer and unique ID.
         */
        constexpr uint32_t asUint() const
        {
            return (static_cast<uint32_t>(mfgId) << 16) | uniqueId;
        }

        /**
         * @brief Equality operator.
         * @param other The other address to compare.
         * @return True if the addresses are equal, false otherwise.
         */
        constexpr bool operator==(const Address &other) const
        {
            return mfgId == other.mfgId && uniqueId == other.uniqueId;
        }

        /**
         * @brief Inequality operator.
         * @param other The other address to compare.
         * @return True if the addresses are not equal, false otherwise.
         */
        constexpr bool operator!=(const Address &other) const
        {
            return !(*this == other);
        }

        /**
         * @brief Serialize the address to a bit stream.
         * @param writer The bit stream writer.
         */
        void serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked<uint8_t>(mfgId);
            writer.write_unchecked<uint16_t>(etl::reverse_bytes<uint16_t>(uniqueId));
        }

        /**
         * @brief Deserialize the address from a bit stream.
         * @param reader The bit stream reader.
         * @return The deserialized address.
         */
        static Address deserialize(etl::bit_stream_reader &reader)
        {
            Address address;
            address.mfgId = reader.read_unchecked<uint8_t>();
            address.uniqueId = etl::reverse_bytes<uint16_t>(reader.read_unchecked<uint16_t>());
            return address;
        }

    };

    
}
