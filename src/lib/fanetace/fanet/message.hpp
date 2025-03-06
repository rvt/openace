#pragma once

#include <stdint.h>
#include "etl/vector.h"
#include "header.hpp"

namespace FANET
{
    /**
     * @brief Message Payload for FANET protocol.
     * Messagetype : 3
     * @tparam SIZE The size of the message payload.
     */
    template <size_t SIZE>
    class MessagePayload final
    {
        uint8_t subHeaderRaw = 0;                // Sub-header raw value
        etl::vector<uint8_t, SIZE> messageRaw = {};   // Raw message data
        static_assert(SIZE <= 244, "MessagePayload size cannot exceed 244 bytes");

    public:
        /**
         * @brief Default constructor.
         */
        MessagePayload() = default;

        /**
         * @brief Constructor with sub-header and message.
         * @param subHeader Sub-header value.
         * @param message Message data.
         */
        MessagePayload(uint8_t subHeader, const etl::vector<uint8_t, SIZE> &message)
            : subHeaderRaw(subHeader), messageRaw(message) {}

        /**
         * @brief Get the message type.
         * @return The message type.
         */
        Header::MessageType type() const
        {
            return Header::MessageType::MESSAGE;
        }

        /**
         * @brief Set the sub-header value.
         * @param subHeader Sub-header value.
         */
        void subHeader(uint8_t subHeader)
        {
            subHeaderRaw = subHeader;
        }

        /**
         * @brief Get the sub-header value.
         * @return The sub-header value.
         */
        uint8_t subHeader() const
        {
            return subHeaderRaw;
        }

        /**
         * @brief Get the message data.
         * @return The message data.
         */
        const etl::ivector<uint8_t> &message() const
        {
            return messageRaw;
        }

        /**
         * @brief Set the message data.
         * @param message The message data.
         */
        void message(const etl::ivector<uint8_t> &message)
        {
            size_t copy_size = std::min(message.size(), messageRaw.capacity());
            messageRaw.assign(message.begin(), message.begin() + copy_size);
        }

        /**
         * @brief Set the message data from an array.
         * @tparam N The size of the array.
         * @param arr The array containing the message data.
         */
        template <size_t N>
        void message(const uint8_t (&arr)[N])
        {
            size_t copy_size = std::min(N, messageRaw.capacity());
            messageRaw.assign(arr, arr + copy_size);
        }

        /**
         * @brief Serialize the message payload to a bit stream.
         * @param writer The bit stream writer.
         */
        virtual void serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked(subHeaderRaw);
            for (auto value : messageRaw)
            {
                writer.write_unchecked(value);
            }
        }

        /**
         * @brief Deserialize the message payload from a bit stream.
         * @param reader The bit stream reader.
         * @return The deserialized message payload.
         */
        static const MessagePayload deserialize(etl::bit_stream_reader &reader)
        {
            MessagePayload payload;
            
            auto subHeaderOpt = reader.read<uint8_t>();
            if (!subHeaderOpt) {
                return payload;
            }
            
            payload.subHeaderRaw = *subHeaderOpt;
            
            while (payload.messageRaw.size() < SIZE) {
                auto byteOpt = reader.read<uint8_t>();
                if (!byteOpt) {
                    break;
                }
                payload.messageRaw.push_back(*byteOpt);
            }

            return payload;
        }
    };
}