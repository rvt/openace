#pragma once

#include <stdint.h>
#include "etl/vector.h"
#include "header.hpp"

namespace FANET
{
    /**
     * Message Payload
     * Messagetype : 3
     */
    template <size_t SIZE>
    class MessagePayload final
    {
        uint8_t subHeaderRaw = 0;
        etl::vector<uint8_t, SIZE> messageRaw;
        static_assert(SIZE <= 244, "MessagePayload size cannot exceed 244 bytes");

    public:
        MessagePayload() = default;

        MessagePayload(uint8_t subHeader, const etl::vector<uint8_t, SIZE> &message)
            : subHeaderRaw(subHeader), messageRaw(message) {}

        Header::MessageType type() const
        {
            return Header::MessageType::MESSAGE;
        }

        void subHeader(uint8_t subHeader)
        {
            subHeaderRaw = subHeader;
        }

        uint8_t subHeader() const
        {
            return subHeaderRaw;
        }

        const etl::ivector<uint8_t> &message() const
        {
            return messageRaw;
        }

        void message(const etl::ivector<uint8_t> &message)
        {
            size_t copy_size = std::min(message.size(), messageRaw.capacity());
            messageRaw.assign(message.begin(), message.begin() + copy_size);
        }

        template <size_t N>
        void message(const uint8_t (&arr)[N])
        {
            size_t copy_size = std::min(N, messageRaw.capacity());
            messageRaw.assign(arr, arr + copy_size);
        }

        virtual void serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked(subHeaderRaw);
            for (auto value : messageRaw)
            {
                writer.write_unchecked(value);
            }
        }

        static const MessagePayload deserialize(etl::bit_stream_reader &reader)
        {
            MessagePayload payload;
            auto available_bytes = reader.size_bytes();
            if (available_bytes < 2) {
                return payload;
            }

            payload.subHeaderRaw = reader.read_unchecked<uint8_t>();
            auto copy_size = std::min(available_bytes - 1, SIZE);
            payload.messageRaw.assign((uint8_t *)(reader.cbegin() + 1), (uint8_t *)(reader.cbegin() + copy_size + 1) );

            return payload;
        }
    };
}