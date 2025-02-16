#pragma once

#include "fanet_packet.hpp"
#include "fanet_common.hpp"
#include "etl/optional.h"
#include "etl/vector.h"

namespace FANET
{
    class PacketBuilder
    {
    private:
        static constexpr size_t MAX_HEADER_SIZE = sizeof(Header) + sizeof(Address) + sizeof(Address) + sizeof(ExtendedHeader) + sizeof(uint32_t);

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

        PacketBuilder &setAck(AckType ackType)
        {
            // Ack may not request an ack, and Ack::NONE does not require an extended header
            if (ackType == AckType::NONE)
            {
                return *this;
            }
            if (extHeader)
            {
                extHeader->ack = ackType;
            }
            else
            {
                extHeader = ExtendedHeader{.reserved = 0, .geoForwarded = false, .signature = false, .cast = false, .ack = ackType};
            }
            return *this;
        }

        PacketBuilder &setDestination(const Address &destination_)
        {
            destination = destination_;
            header.extended = true;
            if (extHeader)
            {
                extHeader->cast = true;
            }
            else
            {
                extHeader = ExtendedHeader{.reserved = 0, .geoForwarded = false, .signature = false, .cast = true, .ack = AckType::NONE};
            }
            return *this;
        }
        PacketBuilder &setDestination(const uint32_t destination_) {
            return setDestination(Address(destination_));
        }

        PacketBuilder &setSignature(uint32_t signature_)
        {
            if (extHeader)
            {
                extHeader->signature = true;
            }
            else
            {
                extHeader = ExtendedHeader{.reserved = 0, .geoForwarded = false, .signature = true, .cast = true, .ack = AckType::NONE};
            }
            signature = signature_;
            header.extended = true;
            return *this;
        }

        ////////////////////////////////

        size_t serializeCommonFields(etl::ivector<uint8_t> &buffer)
        {
            size_t offset = 0;

            buffer.resize(offset + sizeof(Header));
            etl::mem_copy(reinterpret_cast<uint8_t *>(&header), sizeof(Header), buffer.data() + offset);
            offset += sizeof(Header);

            buffer.resize(offset + sizeof(Address));
            etl::mem_copy(reinterpret_cast<uint8_t *>(&source), sizeof(Address), buffer.data() + offset);
            offset += sizeof(Address);

            if (extHeader)
            {
                buffer.resize(offset + sizeof(ExtendedHeader));
                etl::mem_copy(reinterpret_cast<uint8_t *>(&extHeader), sizeof(ExtendedHeader), buffer.data() + offset);
                offset += sizeof(ExtendedHeader);
                if (destination)
                {
                    buffer.resize(offset + sizeof(Address));
                    etl::mem_copy(reinterpret_cast<uint8_t *>(&destination), sizeof(Address), buffer.data() + offset);
                    offset += sizeof(Address);
                }
                if (signature)
                {
                    buffer.resize(offset + sizeof(uint32_t));
                    etl::mem_copy(reinterpret_cast<uint8_t *>(&signature), sizeof(uint32_t), buffer.data() + offset);
                    offset += sizeof(uint32_t);
                }
            }
            return offset;
        }

        etl::vector<uint8_t, MAX_HEADER_SIZE> build()
        {
            etl::vector<uint8_t, MAX_HEADER_SIZE> buffer;
            auto offset = serializeCommonFields(buffer);
            return buffer;
        }

        template <typename PAYLOAD>
        etl::vector<uint8_t, MAX_HEADER_SIZE + sizeof(PAYLOAD)> build(const PAYLOAD &payload)
        {
            etl::vector<uint8_t, MAX_HEADER_SIZE + sizeof(PAYLOAD)> buffer;
            header.type = payload.type();
            auto offset = serializeCommonFields(buffer);
            if (header.type == MessageType::ACK)
            {
                return buffer;
            }
            buffer.resize(offset + sizeof(PAYLOAD));
            etl::mem_copy(const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(&payload)), sizeof(PAYLOAD), buffer.data() + offset);
            return buffer;
        }

    };
};
