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
        uint8_t subHeaderRaw;
        etl::vector<uint8_t, SIZE> messageRaw;
        static_assert(SIZE <= 244, "MessagePayload size cannot exceed 244 bytes");

    public:
        MessagePayload() : subHeaderRaw(0) {}
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
    };
}