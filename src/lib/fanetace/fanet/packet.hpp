#pragma once

#include <stdint.h>
#include "etl/optional.h"
#include "etl/variant.h"
#include "header.hpp"
#include "address.hpp"
#include "tracking.hpp"
#include "name.hpp"
#include "message.hpp"
#include "groundTracking.hpp"
#include "extendedHeader.hpp"

namespace FANET
{
    template <size_t MESSAGESIZE, size_t NAMESIZE>
    using PayloadVariant = etl::variant<TrackingPayload, NamePayload<NAMESIZE>, MessagePayload<MESSAGESIZE>, GroundTrackingPayload>;

    template <size_t MESSAGESIZE, size_t NAMESIZE>
    class Packet
    {
    private:
        void serializeHeader(etl::bit_stream_writer &writer) const
        {
            header_.serialize(writer);
            source_.serialize(writer);

            if (extendedHeader_)
            {
                extendedHeader_->serialize(writer);

                if (destination_)
                {
                    destination_->serialize(writer);
                }
                if (signature_)
                {
                    writer.write_unchecked(etl::reverse_bytes(signature_.value()));
                }
            }
        }

    private:
        Header header_;
        Address source_;
        etl::optional<Address> destination_;
        etl::optional<ExtendedHeader> extendedHeader_;
        etl::optional<uint32_t> signature_;
        etl::optional<PayloadVariant<MESSAGESIZE, NAMESIZE>> payload_;

    public:
        Packet() = default;
        Packet(Header h, Address s, etl::optional<Address> d, etl::optional<ExtendedHeader> e, etl::optional<uint32_t> sig, etl::optional<PayloadVariant<MESSAGESIZE, NAMESIZE>> p)
            : header_(h), source_(s), destination_(d), extendedHeader_(e), signature_(sig), payload_(p) {}

        const Header &header() const { return header_; }
        const Address &source() const { return source_; }
        const etl::optional<Address> &destination() const { return destination_; }
        const etl::optional<ExtendedHeader> &extendedHeader() const { return extendedHeader_; }
        const etl::optional<uint32_t> &signature() const { return signature_; }
        const etl::optional<PayloadVariant<MESSAGESIZE, NAMESIZE>> &payload() const { return payload_; }

        void source(const Address &source)
        {
            source_ = source;
        }

        void ack(ExtendedHeader::AckType ackType)
        {
            // Ack may not request an ack, and Ack::NONE does not require an extended header
            if (ackType == ExtendedHeader::AckType::NONE)
            {
                return *this;
            }
            if (extendedHeader_)
            {
                extendedHeader_->ack(ackType);
            }
            else
            {
                extendedHeader_ = ExtendedHeader{ackType, false, false, false};
            }
            return *this;
        }

        void destination(const Address &destination)
        {
            destination_ = destination;
            if (extendedHeader_)
            {
                extendedHeader_->unicast(true);
            }
            else
            {
                extendedHeader_ = ExtendedHeader{ExtendedHeader::AckType::NONE, true, false, false};
            }
            header_.extended(true);
        }

        void destination(uint32_t dest)
        {
            return destination(Address(dest));
        }

        void signature(uint32_t signature)
        {
            if (extendedHeader_)
            {
                extendedHeader_->signature(true);
            }
            else
            {
                extendedHeader_ = ExtendedHeader{ExtendedHeader::AckType::NONE, false, true, false};
            }
            signature_ = signature;
            header_.extended(true);
        }

        void isGeoForward()
        {
            if (extendedHeader_)
            {
                extendedHeader_->geoForward(true);
            }
            else
            {
                extendedHeader_ = ExtendedHeader{ExtendedHeader::AckType::NONE, false, false, true};
            }
            header_.extended(true);
        }

        void forward(bool forward)
        {
            header_.forward(forward);
        }

        void payLoad(const PayloadVariant<MESSAGESIZE, NAMESIZE> &payload)
        {
            payload_ = payload;
            header_.type(payload.type());
        }

        RadioPacket build()
        {
            RadioPacket buffer;

            if (header_.type() == Header::MessageType::ACK || !payload_)
            {
                return buffer;
            }

            buffer.uninitialized_resize(buffer.capacity());
            etl::bit_stream_writer writer(buffer.data(), buffer.capacity(), etl::endian::big);

            serializeHeader(writer);
            etl::visit([&writer](const auto &payload)
                       { payload.serialize(writer); },
                       *payload_);
            buffer.resize(writer.size_bytes());
            return buffer;
        }

        RadioPacket buildAck()
        {
            RadioPacket buffer;
            header_.type(Header::MessageType::ACK);

            buffer.uninitialized_resize(buffer.capacity());
            etl::bit_stream_writer writer(buffer.data(), buffer.capacity(), etl::endian::big);

            serializeHeader(writer);
            buffer.resize(writer.size_bytes());
            return buffer;
        }
    };

    /**
     * @brief A reference to a packet within a buffer that allows direct inspection or modification without serialization/deserialization.
     * 
     * This class is designed for use by the protocol handler, enabling the manipulation of packets in their raw buffer form rather 
     * than their deserialized state. While deserialization could be used for packet comparison, it introduces unnecessary overhead 
     * and resource consumption. This class allows working with the packet more efficiently.
     * 
     * Notes for future developers:
     * - Only the header and source fields are mandatory. You cannot add for example an `ExtHeader`, but you may modify it if it exists.
     * - The final length of the packet referenced by this class must remain consistent.
     * 
     * You may extend this class with additional functions as needed, but be mindful of the constraints on the packet structure.
     */
    class PacketRef
    {
        friend class Protocol;
        etl::span<uint8_t> packet;
        PacketRef() = default;
        PacketRef(etl::span<uint8_t> packet_) : packet(packet)
        {
        }

        private:
        etl::span<uint8_t> payload() {
            uint8_t payloadPosition[] = {4,4,4,4,5,9,8,12};
            uint8_t extended = (packet[0] & 0x80)?4:0;
            uint8_t cast = ((packet[4] & 0x20) && extended)?2:0;
            uint8_t sig = ((packet[4] & 0x10)) && extended?1:0;
            uint8_t pos = payloadPosition[extended | cast | sig];
            return etl::span<uint8_t>(packet.data() + pos, packet.end());
        }

        /**
         * Returns the mssage type
         */
        Header::MessageType type()  const 
        {
            return static_cast<Header::MessageType>(packet[0] & 0b00111111);
        }

        /**
         * Returns the source address
         */
        Address source()  const 
        {
            return Address(packet[1], (static_cast<uint16_t>(packet[3]) << 8) | static_cast<uint16_t>(packet[2]));
        }

        /**
         * Returns the ackType from the extended header.
         * Returns ExtendedHeader::AckType::NONE if there was no extended header.
         */
        ExtendedHeader::AckType ackType() const 
        {
            if (packet[0] & 0x80)
            {
                return static_cast<ExtendedHeader::AckType>(packet[4] >> 6);
            }
            return ExtendedHeader::AckType::NONE;
        }

        /**
         * Get the destination if within the frame, returns 0 if there is no destination
         */
        Address destination()  const 
        {
            if (packet[0] & 0x80)
            {
                return Address(packet[5], (static_cast<uint16_t>(packet[7]) << 8) | static_cast<uint16_t>(packet[6]));
            }
            return Address{};
        }

        /**
         * Returns the status of the forward bit
         */
        bool forward()  const 
        {
            return packet[0] & 0x40;
        }


        /**
         * Set or reset the forward bit
         */
        void forward(bool forward)
        {
            if (forward)
            {
                packet[0] |= 0x40; // Set bit 6
            }
            else
            {
                packet[0] &= ~0x40; // Clear bit 6
            }
        }
    };
}
