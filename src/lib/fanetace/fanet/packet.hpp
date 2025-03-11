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

        Packet &source(const Address &source)
        {
            source_ = source;
            return *this;
        }

        Packet &singleHop()
        {
            return ack(ExtendedHeader::AckType::SINGLEHOP);
        }

        Packet &twoHop()
        {
            return ack(ExtendedHeader::AckType::TWOHOP);
        }

        Packet &ack(ExtendedHeader::AckType ackType)
        {
            // Ack::NONE does not require an extended header
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
            header_.extended(true);
            return *this;
        }

        Packet &destination(const Address &destination)
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
            return *this;
        }

        Packet &destination(uint32_t dest)
        {
            return destination(Address(dest));
            return *this;
        }

        Packet &signature(uint32_t signature)
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
            return *this;
        }

        Packet &isGeoForward()
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
            return *this;
        }

        Packet &forward()
        {
            header_.forward(true);
            return *this;
        }

        // void payload(const PayloadVariant<MESSAGESIZE, NAMESIZE> &payload)
        // {
        //     payload_ = payload;

        //     //            header_.type(payload..type());
        // }

        Packet &payload(const TrackingPayload &trackingPayload)
        {
            header_.type(trackingPayload.type());
            payload_ = PayloadVariant<MESSAGESIZE, NAMESIZE>(trackingPayload);
            return *this;
        }

        Packet &payload(const GroundTrackingPayload &groundTrackingPayload)
        {
            header_.type(groundTrackingPayload.type());
            payload_ = PayloadVariant<MESSAGESIZE, NAMESIZE>(groundTrackingPayload);
            return *this;
        }

        Packet &payload(const MessagePayload<MESSAGESIZE> &messagePayload)
        {
            header_.type(messagePayload.type());
            payload_ = PayloadVariant<MESSAGESIZE, NAMESIZE>(messagePayload);
            return *this;
        }

        Packet &payload(const NamePayload<NAMESIZE> &namePayload)
        {
            header_.type(namePayload.type());
            payload_ = PayloadVariant<MESSAGESIZE, NAMESIZE>(namePayload);
            return *this;
        }

        RadioPacket build() const
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

        void print() const
        {
            printf("Packet [Type: %d, Src: 0x%6X, Dest: 0x%6X] ",
                   static_cast<int>(header_.type()),
                   source_.asUint(),
                   destination_.value_or(Address{}).asUint());

            extendedHeader_.value_or(ExtendedHeader{}).print();

            printf("\n");
        }
    };
}
