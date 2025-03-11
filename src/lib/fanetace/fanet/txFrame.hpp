#pragma once

#include <stdint.h>
#include "header.hpp"
#include "address.hpp"
#include "extendedHeader.hpp"

namespace FANET
{
    /**
     * @brief Class that holds a raw Packet in the TxPool.
     *
     * Due to the way FANET works, a few header bits can be modified even
     * when they are in the pool. This class supplies that possibility.
     */
    class TxFrame
    {
        friend class Protocol;
        friend class BlockAllocator;

    private:
        etl::span<uint8_t> block_; // Raw packet data, this will point to the correct BEGIN and END in the TxPool after the packet was copied
        uint32_t nextTx_;          // Next transmission time
        uint8_t numTx_;            // Number of transmissions
        int8_t rssi_;              // Received Signal Strength Indicator (RSSI)
        uint16_t id_;              // An app can give a packet an ID. During callbacks the same ID will be returned to indicate that a packet was acked/received etc.

        /**
         * @brief Constructor that initializes the TxFrame with a block of data.
         * @param block The block of data.
         */
        TxFrame(etl::span<uint8_t> block) : block_(block), nextTx_(0), numTx_(0), rssi_(0), id_(0) {}

        /**
         * @brief Set the number of transmissions.
         * @param v The number of transmissions.
         * @return Reference to the current object.
         */
        TxFrame &numTx(uint8_t v)
        {
            numTx_ = v;
            return *this;
        }

        /**
         * @brief ID of this package. Optionally can be used by the application.
         * @param v The ID of this transmission
         * @return Reference to the current object.
         */
        TxFrame &id(uint16_t v)
        {
            id_ = v;
            return *this;
        }

        /**
         * @brief Set the RSSI value.
         * @param v The RSSI value.
         * @return Reference to the current object.
         */
        TxFrame &rssi(int16_t v)
        {
            rssi_ = v;
            return *this;
        }

        /**
         * @brief Set the next transmission time.
         * @param v The next transmission time.
         * @return Reference to the current object.
         */
        TxFrame &nextTx(uint32_t v)
        {
            nextTx_ = v;
            return *this;
        }

        /**
         * @brief Set or reset the forward bit.
         * @param forward True to set the forward bit, false to reset it.
         */
        TxFrame &forward(bool forward)
        {
            if (forward)
            {
                block_[0] |= 0x40; // Set bit 6
            }
            else
            {
                block_[0] &= ~0x40; // Clear bit 6
            }

            return *this;
        }

        /**
         * @brief Get the payload data.
         * @return The payload data as a span.
         */
        etl::span<uint8_t> payload()
        {
            // Position of the payload in the packet based on the various bits in the header
            uint8_t payloadPosition[] = {4, 4, 4, 4, 5, 9, 8, 12};
            uint8_t extended = (block_[0] & 0x80) ? 4 : 0;
            uint8_t cast = ((block_[4] & 0x20) && extended) ? 2 : 0;
            uint8_t sig = ((block_[4] & 0x10)) && extended ? 1 : 0;
            uint8_t pos = payloadPosition[extended | cast | sig];
            return etl::span<uint8_t>(block_.data() + pos, block_.end());
        }

    public:
        /**
         * @brief Get the next transmission time.
         * @return The next transmission time.
         */
        uint32_t nextTx() const
        {
            return nextTx_;
        }

        /**
         * @brief Get the ID of this TX packet
         * @return The ID
         */
        uint16_t id() const
        {
            return id_;
        }

        /**
         * @brief Get the number of transmissions.
         * @return The number of transmissions.
         */
        uint8_t numTx() const
        {
            return numTx_;
        }

        /**
         * @brief Get the RSSI value.
         * @return The RSSI value.
         */
        int16_t rssi() const
        {
            return rssi_;
        }

        /**
         * @brief Get the source address.
         * @return The source address.
         */
        Address source() const
        {
            return Address(block_[1], (static_cast<uint16_t>(block_[3]) << 8) | static_cast<uint16_t>(block_[2]));
        }

        /**
         * @brief Get the destination address.
         * @return The destination address.
         */
        Address destination() const
        {
            // Test for CAST bit in the extended header
            if ((block_[0] & 0x80) && (block_[4] & 0x20))
            {
                return Address(block_[5], (static_cast<uint16_t>(block_[7]) << 8) | static_cast<uint16_t>(block_[6]));
            }
            return Address{};
        }

        /**
         * @brief Get the message type.
         * @return The message type.
         */
        Header::MessageType type() const
        {
            return static_cast<Header::MessageType>(block_[0] & 0b00111111);
        }

        /**
         * @brief Check if the packet is a priority packet.
         * @return True if the packet is a priority packet, false otherwise.
         */
        bool isPriorityPacket() const
        {
            auto t = type();
            return t == Header::MessageType::GROUND_TRACKING || t == Header::MessageType::TRACKING;
        }

        /**
         * @brief Get the status of the forward bit.
         * @return True if the forward bit is set, false otherwise.
         */
        bool forward() const
        {
            return block_[0] & 0x40;
        }

        /**
         * @brief Get the acknowledgment type.
         * @return The acknowledgment type.
         */
        ExtendedHeader::AckType ackType() const
        {
            if (block_[0] & 0x80)
            {
                return static_cast<ExtendedHeader::AckType>(block_[4] >> 6);
            }
            return ExtendedHeader::AckType::NONE;
        }

    public:
        /**
         * @brief Get the raw packet data.
         * @return The raw packet data as a span.
         */
        etl::span<uint8_t> data() const
        {
            return block_;
        }

        /**
         * @brief Set the raw packet data.
         * @param v The raw packet data as a span.
         */
        void data(etl::span<uint8_t> v)
        {
            block_ = v;
        }
    };
}
