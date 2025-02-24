#pragma once

#include "fanet.hpp"
#include "etl/optional.h"
#include "etl/vector.h"
#include "etl/bit_stream.h"

namespace FANET
{
    class PacketParser final 
    {
    private:
        Header header{};
        Address source{};
        etl::optional<Address> destination;
        etl::optional<ExtendedHeader> extHeader;
        etl::optional<uint32_t> signature;

        PacketParser() = default;
    public:



        // const PacketParser &parse(const RadioPacket &packet) {
        //     PacketParser packet;

        //     etl::bit_stream_reader reader(packet.data(), packet.size(), etl::endian::big);

        //     static const PacketParser INVALID_FRAME; // Shared invalid frame
        //     PacketParser frame;
        //     size_t index = 0;

        //     if (packet.size() < sizeof(Header) + sizeof(Address))
        //     {
        //         return INVALID_FRAME;
        //     }

        //     header = Header.deserialize(reader);
        //     source = Address.deserialize(reader);

        //     if (header.extended())
        //     {

        //     //     ExtendedHeader extHeader;
        //     //     etl::mem_copy(reinterpret_cast<uint8_t *>(buffer[index]), index + sizeof(ExtendedHeader), reinterpret_cast<uint8_t *>(&frame.extHeader));
        //     //     index += sizeof(ExtendedHeader);

        //     //     if (frame.extHeader.cast())
        //     //     {
        //     //         if (bufferSize < index + sizeof(Address))
        //     //         {
        //     //             return INVALID_FRAME;
        //     //         }

        //     //         etl::mem_copy(reinterpret_cast<uint8_t *>(buffer[index]), index + sizeof(Address), reinterpret_cast<uint8_t *>(&frame.destination));
        //     //         index += sizeof(Address);
        //     //     }

        //     //     if (frame.extHeader.signature)
        //     //     {
        //     //         if (bufferSize < index + sizeof(uint32_t))
        //     //         {
        //     //             return INVALID_FRAME;
        //     //         }

        //     //         etl::mem_copy(reinterpret_cast<uint8_t *>(buffer[index]), index + sizeof(uint32_t), reinterpret_cast<uint8_t *>(&frame.signature));
        //     //         index += sizeof(uint32_t);
        //     //     }
        //     }

        //     // if (index < bufferSize)
        //     // {
        //     //     frame.payload.assign(buffer + index, buffer + bufferSize);
        //     // }

        //     // frame.isValid = true;
        //     return frame;
        // }
    };
}