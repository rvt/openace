#pragma once

#include <stdint.h>
#include "etl/bit_stream.h"
#include "etl/vector.h"

namespace FANET
{
    using RadioPacket = etl::vector<uint8_t, 255>;

    class Payloadbase
    {
    public:
        virtual void serialize(etl::bit_stream_writer &writer) const = 0;
    };

    class Header final
    {
    public:
        enum class MessageType : uint8_t
        {
            ACK = 0,
            TRACKING = 1,
            NAME = 2,
            MESSAGE = 3,
            SERVICE = 4,
            LANDMARKS = 5,
            REMOTE_CONFIG = 6,
            GROUND_TRACKING = 7
        };

    private:
        bool hasExtended = false;               // Extended header bit
        bool isForwarded = false;               // Forwarding bit
        MessageType msgType = MessageType::ACK; // Type of message (6 bits)

    public:
    explicit Header() = default;

        Header(bool extended_, bool forward_, MessageType type_)
            : hasExtended(extended_), isForwarded(forward_), msgType(type_) {}

        MessageType type() const
        {
            return msgType;
        }
        void type(MessageType value)
        {
            msgType = value;
        }

        bool forward() const
        {
            return isForwarded;
        }
        void forward(bool value)
        {
            isForwarded = value;
        }

        bool extended() const
        {
            return hasExtended;
        }
        void extended(bool value)
        {
            hasExtended = value;
        }

        void serialize(etl::bit_stream_writer &writer) const
        {
            writer.write_unchecked(hasExtended);
            writer.write_unchecked(isForwarded);
            writer.write_unchecked(static_cast<uint8_t>(msgType), 6U);
        }

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