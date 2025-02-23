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

        size_t headerSize()
        {
            // TODO Take the length from each object
            size_t headerLength = 4; // Header + source Address
            if (extHeader)
            {
                headerLength++;
                if (destination)
                {
                    // Address = 3 bytes
                    headerLength = headerLength + 3;
                }
                if (signature)
                {
                    // Signature = 4 bytes
                    headerLength = headerLength + 4;
                }
            }
            return headerLength;
        }

        void serialize(etl::bit_stream_writer &writer) const
        {
            header.serialize(writer);
            source.serialize(writer);

            if (extHeader)
            {
                extHeader->serialize(writer);

                if (destination)
                {
                    destination->serialize(writer);
                }
                if (signature)
                {
                    writer.write_unchecked(etl::reverse_bytes(signature.value()));
                }
            }
        }

    private:
        Header header{};
        Address source{};

        etl::optional<Address> destination;
        etl::optional<ExtendedHeader> extHeader;
        etl::optional<uint32_t> signature;

    public:
        PacketBuilder(uint32_t src) : PacketBuilder(Address(src))
        {
        }

        PacketBuilder(const Address &src) : source{src}
        {
        }

        PacketBuilder &setAck(ExtendedHeader::AckType ackType)
        {
            // Ack may not request an ack, and Ack::NONE does not require an extended header
            if (ackType == ExtendedHeader::AckType::NONE)
            {
                return *this;
            }
            if (extHeader)
            {
                extHeader->ack(ackType);
            }
            else
            {
                extHeader = ExtendedHeader{0, false, false, false, ackType};
            }
            return *this;
        }

        PacketBuilder &setDestination(const Address &destination_)
        {
            destination = destination_;
            if (extHeader)
            {
                extHeader->unicast(true);
            }
            else
            {
                extHeader = ExtendedHeader{0, false, false, true, ExtendedHeader::AckType::NONE};
            }
            header.extended(true);
            return *this;
        }

        PacketBuilder &setDestination(const uint32_t destination_)
        {
            return setDestination(Address(destination_));
        }

        PacketBuilder &setSignature(uint32_t signature_)
        {
            if (extHeader)
            {
                extHeader->signature(true);
            }
            else
            {
                extHeader = ExtendedHeader{0, false, true, false, ExtendedHeader::AckType::NONE};
            }
            signature = signature_;
            header.extended(true);
            return *this;
        }

        ////////////////////////////////
        template<typename PAYLOAD>
        // etl::vector<uint8_t, MAX_HEADER_SIZE + sizeof(PAYLOAD)> build(const PAYLOAD &payload)
        RadioPacket build(const PAYLOAD &payload)
        {
            RadioPacket buffer;
            etl::bit_stream_writer writer(buffer.data(), buffer.capacity(), etl::endian::big);

            header.type(payload.type());
            serialize(writer);

            // AckPayload cannot be serialized as it's header only
            if (header.type() == Header::MessageType::ACK)
            {
                // When no destination, simply return an empty buffer
                if (!destination) {
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
    };
};
