#pragma once

#include <stdint.h>

#include "header.hpp"
#include "address.hpp"
#include "extendedHeader.hpp"

namespace FANET
{

    /**
     * Class that holds a raw Packet in the TxPool
     * Due to teh way FANET works, a few header bits can be modified even
     * when they are in the pool.AckPayloadTHis class in addition supplies that posibility.
     */
    class TxFrame
    {
        friend class Protocol;
        friend class BlockAllocator;

    private:
        etl::span<uint8_t> block_;
        uint32_t nextTx_;
        uint8_t numTx_;
        int16_t rssi_;

        TxFrame(etl::span<uint8_t> block) : block_(block), nextTx_(0), numTx_(0), rssi_(0) {}

        TxFrame &numTx(uint8_t v)
        {
            numTx_ = v;
            return *this;
        }
        TxFrame &rssi(int16_t v)
        {
            rssi_ = v;
            return *this;
        }
        TxFrame &nextTx(uint32_t v)
        {
            nextTx_ = v;
            return *this;
        }
        uint32_t nextTx() const
        {
            return nextTx_;
        }
        uint8_t numTx() const
        {
            return numTx_;
        }
        uint8_t rssi() const
        {
            return rssi_;
        }

        /**
         * Returns the source address
         */
        Address source() const
        {
            return Address(block_[1], (static_cast<uint16_t>(block_[3]) << 8) | static_cast<uint16_t>(block_[2]));
        }

        Address destination() const
        {
            // Test for CAST bit in teh extended header
            if ((block_[0] & 0x80) && (block_[4] & 0x20))
            {
                return Address(block_[5], (static_cast<uint16_t>(block_[7]) << 8) | static_cast<uint16_t>(block_[6]));
            }
            return Address{};
        }

        Header::MessageType type() const
        {
            return static_cast<Header::MessageType>(block_[0] & 0b00111111);
        }

        bool isPriorityPacket() const
        {
            auto t = type();
            return t == Header::MessageType::GROUND_TRACKING || t == Header::MessageType::TRACKING;
        }

        /**
         * Returns the status of the forward bit
         */
        bool forward() const
        {
            return block_[0] & 0x40;
        }

        /**
         * Set or reset the forward bit
         */
        void forward(bool forward)
        {
            if (forward)
            {
                block_[0] |= 0x40; // Set bit 6
            }
            else
            {
                block_[0] &= ~0x40; // Clear bit 6
            }
        }

        ExtendedHeader::AckType ackType() const
        {
            if (block_[0] & 0x80)
            {
                return static_cast<ExtendedHeader::AckType>(block_[4] >> 6);
            }
            return ExtendedHeader::AckType::NONE;
        }

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
        etl::span<uint8_t> data() const
        {
            return block_;
        }
        void data(etl::span<uint8_t> v)
        {
            block_ = v;
        }

    };
}
