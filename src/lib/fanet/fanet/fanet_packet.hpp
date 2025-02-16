#pragma once

#include "fanet_common.hpp"
#include <cstdint>
#include <etl/vector.h>
#include <etl/array.h>
#include <etl/memory.h>

namespace FANET
{
    class FanetRxPacket
    {
        Header header{};
        Address source{};
        ExtendedHeader extHeader{};        // Present if header.extended is set
        Address destination{};             // Present if extHeader.cast is set
        uint32_t signature{};              // Present if extHeader.signature is set
        etl::vector<uint8_t, 255> payload; // Message payload
        bool isValid{false};               // Indicates if parsing was successful

    public:
        FanetRxPacket() = default;

        /**
         * Create a fanet frame from a byte buffer
         * The caller must validate the valid vlag after creating the FanetFrame
         */
        static const FanetRxPacket fromBuffer(const uint8_t *buffer, size_t bufferSize) 
        {
            static const FanetRxPacket INVALID_FRAME; // Shared invalid frame
            FanetRxPacket frame;
            frame.isValid = false;

            size_t index = 0;

            if (bufferSize < sizeof(Header) + sizeof(Address))
            {
                return INVALID_FRAME;
            }

            etl::mem_copy(reinterpret_cast<uint8_t *>(buffer[index]), index + sizeof(Header), reinterpret_cast<uint8_t *>(&frame.header));
            index += sizeof(Header);

            etl::mem_copy(reinterpret_cast<uint8_t *>(buffer[index]), index + sizeof(Address), reinterpret_cast<uint8_t *>(&frame.source));
            index += sizeof(Address);

            if (frame.header.extended)
            {
                if (bufferSize < index + sizeof(ExtendedHeader))
                {
                    return INVALID_FRAME;
                }

                etl::mem_copy(reinterpret_cast<uint8_t *>(buffer[index]), index + sizeof(ExtendedHeader), reinterpret_cast<uint8_t *>(&frame.extHeader));
                index += sizeof(ExtendedHeader);

                if (frame.extHeader.cast)
                {
                    if (bufferSize < index + sizeof(Address))
                    {
                        return INVALID_FRAME;
                    }

                    etl::mem_copy(reinterpret_cast<uint8_t *>(buffer[index]), index + sizeof(Address), reinterpret_cast<uint8_t *>(&frame.destination));
                    index += sizeof(Address);
                }

                if (frame.extHeader.signature)
                {
                    if (bufferSize < index + sizeof(uint32_t))
                    {
                        return INVALID_FRAME;
                    }

                    etl::mem_copy(reinterpret_cast<uint8_t *>(buffer[index]), index + sizeof(uint32_t), reinterpret_cast<uint8_t *>(&frame.signature));
                    index += sizeof(uint32_t);
                }
            }

            if (index < bufferSize)
            {
                frame.payload.assign(buffer + index, buffer + bufferSize);
            }

            frame.isValid = true;
            return frame;
        }

        bool valid() const { return isValid; }
    };

    
};