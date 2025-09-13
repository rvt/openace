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
    Configuration &config;

public:
    CobsStreamHandler(etl::imessage_bus &bus_, Configuration &config_) : bus(bus_), config(config_)
    {
    }

    void handle(float ownShipLat, float ownShipLon, etl::span<uint8_t> cobsBuffer)
    {
        etl::vector<GATAS::AircraftPositionInfo, 8> positionMessages;
        buffer.set(cobsBuffer);
        etl::span<uint8_t> data;
        while (buffer.read(data))
        {
            // There could be null entries
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
                auto aircraftPosition = BinaryMessages::deserializeAircraftPositionV1(ownShipLat, ownShipLon, reader);
                if (positionMessages.full())
                {
                    bus.receive(GATAS::AircraftPositionsMsg(positionMessages));
                    positionMessages.clear();
                }
                positionMessages.push_back(aircraftPosition);
            }

            if (frameType == BinaryMessages::DataType::SET_ICAO_ADDRESS_V1)
            {
                auto icaoAddress = BinaryMessages::deserializeSetIcaoAddressV1(reader);
                if (icaoAddress != 0)
                {
                    auto current = config.gaTasConfig();
                    auto callSign = config.getCallSignFromHex(icaoAddress);
                    if (!callSign.empty() && current.conspicuity.icaoAddress != icaoAddress)
                    {
                        config.setValueBypath("config/aircraftId", callSign);

                        // Tell attached systems to load any new data
                        bus.receive(
                            GATAS::ConfigUpdatedMsg{
                                config,
                                Configuration::CONFIG,
                            });
                    }
                }
            }
        }
        // Send the left over if any
        if (!positionMessages.empty())
        {
            bus.receive(GATAS::AircraftPositionsMsg(positionMessages));
        }
    }

    void clear()
    {
        buffer.clear();
    }
};