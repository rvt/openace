#pragma once

#include <stdint.h>
#include "etl/bit_stream.h"
#include "etl/vector.h"

namespace FANET
{
    using RadioPacket = etl::vector<uint8_t, 255>;

    /**
     * @brief Base class for all payloads.
     */
    class Payloadbase
    {
    public:
        /**
         * @brief Serialize the payload to a bit stream.
         * @param writer The bit stream writer.
         */
        virtual void serialize(etl::bit_stream_writer &writer) const = 0;
    };

    /**
     * @brief Represents the header for FANET protocol.
     */
    class Header final
    {
    public:
        /**
         * @brief Enumeration for message types.
         */
        enum class MessageType : uint8_t
        {
            ACK = 0,            // Acknowledgment
            TRACKING = 1,       // Tracking
            NAME = 2,           // Name
            MESSAGE = 3,        // Message
            SERVICE = 4,        // Service
            LANDMARKS = 5,      // Landmarks
            REMOTE_CONFIG = 6,  // Remote configuration
            GROUND_TRACKING = 7 // Ground tracking
        };

    private:
        bool hasExtended = false;               // Extended header bit
        bool isForwarded = false;               // Forwarding bit
        MessageType msgType = MessageType::ACK; // Type of message (6 bits)

    public:
        /**
         * @brief Default constructor.
         */
        explicit Header() = default;

        /**
         * @brief Constructor with all fields.
         * @param extended_ Extended header bit.
         * @param forward_ Forwarding bit.
         * @param type_ Message type.
         */
        Header(bool extended_, bool forward_, MessageType type_)
            : hasExtended(extended_), isForwarded(forward_), msgType(type_) {}

        /**
         * @brief Get the message type.
         * @return The message type.
         */
        MessageType type() const
        {
            return msgType;
        }

        /**
         * @brief Set the message type.
         * @param value The message type.
         */
        void type(MessageType value)
        {
            msgType = value;
        }

        /**
         * @brief Get the forwarding bit.
         * @return True if forwarding is enabled, false otherwise.
         */
        bool forward() const
        {
            return isForwarded;
        }

        /**
         * @brief Set the forwarding bit.
         * @param value True to enable forwarding, false to disable.
         */
        void forward(bool value)
        {
            isForwarded = value;
        }

        /**
         * @brief Get the extended header bit.
         * @return True if extended header is present, false otherwise.
         */
        bool extended() const
        {
            return hasExtended;
        }

        /**
         * @brief Set the extended header bit.
         * @param value True to indicate extended header, false to indicate no extended header.
         */
        void extended(bool value)
        {
            hasExtended = value;
        }

        /**
         * @brief Serialize the header to a bit stream.
         * @param writer The bit stream writer.
         */
        void serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked(hasExtended);
            writer.write_unchecked(isForwarded);
            writer.write_unchecked(static_cast<uint8_t>(msgType), 6U);
        }

        /**
         * @brief Deserialize the header from a bit stream.
         * @param reader The bit stream reader.
         * @return The deserialized header.
         */
        static const Header deserialize(etl::bit_stream_reader &reader)
        {
            Header header;
            header.hasExtended = reader.read_unchecked<bool>();
            header.isForwarded = reader.read_unchecked<bool>();
            header.msgType = static_cast<MessageType>(reader.read_unchecked<uint8_t>(6U));
            return header;
        }
    };

}