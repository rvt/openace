#pragma once
#include <cstddef>
#include "cobs.hpp"
#include "streambuffer.hpp"
#include "ace/binarymessages.hpp"
#include "ace/messages.hpp"
#include "etl/message_bus.h"
#include "etl/span.h"

class CobsStreamHandler
{
private:
    StreamBuffer<64, 0> buffer;
    etl::imessage_bus &bus;

public:
    CobsStreamHandler(etl::imessage_bus &bus_) : bus(bus_)
    {
    }

    void handle(float ownShipLat, float ownShipLon, etl::span<uint8_t> cobsBuffer)
    {
        etl::vector<GATAS::AircraftPositionInfo, 8> positionMessages;

        buffer.set(cobsBuffer);
        etl::span<uint8_t> data;
        while (buffer.read(data))
        {
            if (data.size() == 0)
            {
                continue;
            }

            decodeCOBS_inplace(data);
            etl::bit_stream_reader reader(data, etl::endian::big);
            uint8_t frameType = reader.read_unchecked<uint8_t>(8U);
            reader.restart();
            if (frameType == BinaryMessages::DataType::AIRCRAFT_POSITION_TYPE_V1)
            {
                auto aircraftPosition = BinaryMessages::AircraftPositionInfo(ownShipLat,ownShipLon,reader);
                if (positionMessages.full())
                {
                    bus.receive(GATAS::AircraftPositionsMsg(positionMessages));
                    positionMessages.clear();
                }
                positionMessages.push_back(aircraftPosition);
            }
        }
        // Send the left over if any
        if (!positionMessages.empty())
        {
            bus.receive(GATAS::AircraftPositionsMsg(positionMessages));
        }
    }

    void clear() {
        buffer.clear();
    }
};