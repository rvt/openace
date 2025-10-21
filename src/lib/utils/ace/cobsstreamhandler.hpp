#pragma once
#include <cstddef>
#include "cobs.hpp"
#include "gulp.hpp"
#include "../old/streambuffer.hpp"
#include "ace/binarymessages.hpp"
#include "ace/messages.hpp"
#include "etl/message_bus.h"
#include "etl/span.h"
#include "etl/vector.h"

/**
 * Handle incoming COBS messages and call the correct function the handle the datasets
 */
class CobsStreamHandler
{
private:
    etl::imessage_bus &bus;
    Configuration &config;
    etl::vector<uint8_t, 32> gulpBuffer;
    Gulp gulp;

public:
    CobsStreamHandler(etl::imessage_bus &bus_, Configuration &config_) : bus(bus_), config(config_), gulp(gulpBuffer, DelimiterBitmap::Null())
    {
    }

    void handle(float ownShipLat, float ownShipLon, etl::span<uint8_t> cobsBuffer)
    {
        gulp.setRef(cobsBuffer);
        etl::vector<GATAS::AircraftPositionInfo, 8> positionMessages;

        etl::span<uint8_t> data;
        while (gulp.pop_into(data))
        {
            decodeCOBS_inplace(data);
            uint8_t frameType = data[0];
            etl::bit_stream_reader reader(data, etl::endian::big);

            /**
             * Handle a aircraft that's received
             */
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

            /**
             * Handle change of current selected aircraft
             */
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
};