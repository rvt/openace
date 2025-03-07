#pragma once

#include "fanet.hpp"
#include "etl/optional.h"
#include "etl/vector.h"
#include "etl/bit_stream.h"
#include "etl/variant.h"
#include "header.hpp"
#include "address.hpp"
#include "tracking.hpp"
#include "name.hpp"
#include "message.hpp"
#include "groundTracking.hpp"

namespace FANET
{
    /**
     * @brief A variant type to hold different payload types.
     * @tparam MESSAGESIZE The size of the message payload.
     * @tparam NAMESIZE The size of the name payload.
     */
    template <size_t MESSAGESIZE, size_t NAMESIZE>
    using PayloadVariant = etl::variant<TrackingPayload, NamePayload<NAMESIZE>, MessagePayload<MESSAGESIZE>, GroundTrackingPayload>;

    /**
     * @brief A class to parse FANET packets from a byte buffer.
     * 
     * The PacketParser class is responsible for parsing a byte buffer into a FANET packet.
     * It reads the header, source address, optional extended header, and payload from the buffer.
     * The class supports different payload types based on the message type in the header.
     * 
     * @tparam MESSAGESIZE The size of the message payload.
     * @tparam NAMESIZE The size of the name payload.
     */
    template <size_t MESSAGESIZE, size_t NAMESIZE>
    class PacketParser final
    {
    public:
        /**
         * @brief Parse a byte buffer into a FANET packet.
         * 
         * This function reads the header, source address, optional extended header, and payload from the buffer.
         * It supports different payload types based on the message type in the header.
         * 
         * @param buffer The byte buffer containing the packet data.
         * @return The parsed FANET packet.
         */
        static Packet<MESSAGESIZE, NAMESIZE> parse(const etl::ivector<uint8_t> &buffer)
        {
            etl::bit_stream_reader reader((uint8_t *)buffer.data(), buffer.size(), etl::endian::big);
            Header header;
            Address source;
            etl::optional<Address> optDestination;
            etl::optional<ExtendedHeader> optExtHeader;
            etl::optional<uint32_t> optSignature;
            etl::optional<PayloadVariant<MESSAGESIZE, NAMESIZE>> optPayload;

            reader.restart();
            header = Header::deserialize(reader);
            source = Address::deserialize(reader);
            if (header.extended())
            {
                optExtHeader = ExtendedHeader::deserialize(reader);

                if (optExtHeader->unicast())
                {
                    optDestination = Address::deserialize(reader);
                }
                if (optExtHeader->signature())
                {
                    optSignature = etl::reverse_bytes(reader.read_unchecked<uint32_t>());
                }
            }

            switch (header.type())
            {
            case Header::MessageType::TRACKING:
                optPayload = TrackingPayload::deserialize(reader);
                break;
            case Header::MessageType::NAME:
                optPayload = NamePayload<NAMESIZE>::deserialize(reader);
                break;
            case Header::MessageType::MESSAGE:
                optPayload = MessagePayload<MESSAGESIZE>::deserialize(reader);
                break;
            case Header::MessageType::GROUND_TRACKING:
                optPayload = GroundTrackingPayload::deserialize(reader);
                break;
            default:
                break; // ACK or unsupported types
            }

            return Packet<MESSAGESIZE, NAMESIZE>(header, source, optDestination, optExtHeader, optSignature, optPayload);
        }
    };
};
