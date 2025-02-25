#pragma once

#include "fanet.hpp"
#include "etl/optional.h"
#include "etl/vector.h"

namespace FANET
{
    class PacketBuilder final 
    {
    private:
        static constexpr size_t MAX_HEADER_SIZE = sizeof(Header) + sizeof(Address) + sizeof(Address) + sizeof(ExtendedHeader) + sizeof(uint32_t);

        // size_t headerSize()
        // {
        //     // TODO Take the length from each object
        //     size_t headerLength = 4; // Header + source Address
        //     if (optExtHeader)
        //     {
        //         headerLength++;
        //         if (optDestination)
        //         {
        //             // Address = 3 bytes
        //             headerLength = headerLength + 3;
        //         }
        //         if (optSignature)
        //         {
        //             // Signature = 4 bytes
        //             headerLength = headerLength + 4;
        //         }
        //     }
        //     return headerLength;
        // }

        void serialize(etl::bit_stream_writer &writer) const
        {
            header.serialize(writer);
            source.serialize(writer);

            if (optExtHeader)
            {
                optExtHeader->serialize(writer);

                if (optDestination)
                {
                    optDestination->serialize(writer);
                }
                if (optSignature)
                {
                    writer.write_unchecked(etl::reverse_bytes(optSignature.value()));
                }
            }
        }

    private:
        Header header{};
        Address source{};

        etl::optional<Address> optDestination;
        etl::optional<ExtendedHeader> optExtHeader;
        etl::optional<uint32_t> optSignature;

    public:
        PacketBuilder(uint32_t src) : PacketBuilder(Address(src))
        {
        }

        PacketBuilder(const Address &src) : source{src}
        {
        }

        PacketBuilder &ack(ExtendedHeader::AckType ackType)
        {
            // Ack may not request an ack, and Ack::NONE does not require an extended header
            if (ackType == ExtendedHeader::AckType::NONE)
            {
                return *this;
            }
            if (optExtHeader)
            {
                optExtHeader->ack(ackType);
            }
            else
            {
                optExtHeader = ExtendedHeader{ackType, false, false, false};
            }
            return *this;
        }

        PacketBuilder &destination(const Address &destination_)
        {
            optDestination = destination_;
            if (optExtHeader)
            {
                optExtHeader->unicast(true);
            }
            else
            {
                optExtHeader = ExtendedHeader{ExtendedHeader::AckType::NONE, true, false, false};
            }
            header.extended(true);
            return *this;
        }

        PacketBuilder &destination(const uint32_t destination_)
        {
            return destination(Address(destination_));
        }

        PacketBuilder &signature(uint32_t signature_)
        {
            if (optExtHeader)
            {
                optExtHeader->signature(true);
            }
            else
            {
                optExtHeader = ExtendedHeader{ExtendedHeader::AckType::NONE, false, true, false};
            }
            optSignature = signature_;
            header.extended(true);
            return *this;
        }

        PacketBuilder &isGeoForward()
        {
            if (optExtHeader)
            {
                optExtHeader->geoForward(true);
            }
            else
            {
                optExtHeader = ExtendedHeader{ExtendedHeader::AckType::NONE, false, false, true};
            }
            header.extended(true);
            return *this;
        }

        PacketBuilder &isForward()
        {
            header.forward(true);
            return *this;
        }

        ////////////////////////////////
        template<typename PAYLOAD>
        // etl::vector<uint8_t, MAX_HEADER_SIZE + sizeof(PAYLOAD)> build(const PAYLOAD &payload)
        RadioPacket build(const PAYLOAD &payload)
        {
            RadioPacket buffer;
            buffer.resize(buffer.capacity());
            etl::bit_stream_writer writer(buffer.data(), buffer.capacity(), etl::endian::big);

            header.type(payload.type());
            serialize(writer);

            // AckPayload cannot be serialized as it's header only
            if (header.type() == Header::MessageType::ACK)
            {
                // When no destination, simply return an empty buffer
                if (!optExtHeader) {
                    buffer.clear();
                    return buffer;
                }
                buffer.resize(writer.size_bytes());
                return buffer;
            }

            // serialize the payload
            payload.serialize(writer);
            buffer.resize(writer.size_bytes());
            return buffer;
        }

        RadioPacket buildAck(Address destination_)
        {
            RadioPacket buffer;

            // If this is not a ack type, then return an empty buffer
            if (header.type() != Header::MessageType::ACK)
            {
                buffer.clear();
                return buffer;
            }
            header.type(Header::MessageType::ACK);
            destination(destination_);
            buffer.resize(buffer.capacity());
            etl::bit_stream_writer writer(buffer.data(), buffer.capacity(), etl::endian::big);

            serialize(writer);
            buffer.resize(writer.size_bytes());
            return buffer;
        }
    };
};
