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
        MessageType msgType : 6; // Type of message (6 bits)
        bool isForwarded : 1;    // Forwarding bit
        bool hasExtended : 1;    // Extended header bit

    public:
        // Constructor
        Header() : msgType(static_cast<MessageType>(0)), isForwarded(false), hasExtended(false) {}

        Header(MessageType type_, bool forward_, bool extended_)
            : msgType(type_), isForwarded(forward_), hasExtended(extended_) {}

        // Methods without `get` and `set` prefixes
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

        void deserialize(etl::bit_stream_reader &reader)
        {
            hasExtended = reader.read_unchecked<bool>();
            isForwarded = reader.read_unchecked<bool>();
            msgType = static_cast<MessageType>(reader.read_unchecked<uint8_t>(6U));
        }
    } __attribute__((packed));

}