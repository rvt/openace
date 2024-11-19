#include <stdio.h>

#include <math.h>
#include "flarm2024.hpp"
#include "flarm_utils.hpp"
#include "ace/manchester.hpp"

inline uint32_t MX(uint32_t z, uint32_t y, uint32_t sum, uint8_t e, uint8_t p, const etl::span<const uint32_t> &keys)
{
    return (((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ ((sum ^ y) + (keys[(p & 3) ^ e] ^ z));
}

void Flarm2024::bteaEncode(uint32_t *data) const
{
    auto rounds = BTEA_ROUNDS;
    uint32_t sum = 0;
    uint32_t z = data[BTEA_N - 1];
    do
    {
        uint32_t y;
        sum += BTEA_DELTA;
        uint8_t e = (sum >> 2) & 3;
        for (uint8_t p = 0; p < BTEA_N - 1; p++)
        {
            y = data[p + 1];
            z = data[p] += MX(z, y, sum, e, p, BTEA_KEYS);
        }
        y = data[0];
        z = data[BTEA_N - 1] += MX(z, y, sum, e, BTEA_N - 1, BTEA_KEYS);
    } while (--rounds);
}

void Flarm2024::bteaDecode(uint32_t *data) const
{
    auto rounds = BTEA_ROUNDS;
    uint32_t sum = rounds * BTEA_DELTA;
    uint32_t y = data[0];
    do
    {
        uint32_t z;
        uint8_t e = (sum >> 2) & 3;
        for (uint8_t p = BTEA_N - 1; p > 0; p--)
        {
            z = data[p - 1];
            y = data[p] -= MX(z, y, sum, e, p, BTEA_KEYS);
        }
        z = data[BTEA_N - 1];
        y = data[0] -= MX(z, y, sum, e, 0, BTEA_KEYS);
        sum -= BTEA_DELTA;
    } while (--rounds);
}


void Flarm2024::scramble(uint32_t *data, uint32_t timestamp) const
{
    constexpr uint8_t numKeys = 4;
    constexpr uint8_t numIterations = 2;
    constexpr uint8_t byteLength = numKeys * sizeof(uint32_t);

    uint32_t wkeys[] = {data[0], data[1], timestamp >> 4, SCRAMBLE};
    uint8_t *bkeys = reinterpret_cast<uint8_t *>(&wkeys);

    uint16_t y, x;
    uint8_t z = bkeys[byteLength - 1];
    uint32_t sum = 0;

    for (auto q = 0; q < numIterations; ++q)
    {
        sum += BTEA_DELTA;
        y = bkeys[0];
        for (uint8_t p = 0; p < byteLength - 1; ++p)
        {
            x = y;
            y = bkeys[p + 1];
            x += ((((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ (sum ^ y));
            bkeys[p] = static_cast<uint8_t>(x);
            z = x & 0xff;
        }
        x = y;
        y = bkeys[0];
        x += ((((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ (sum ^ y));
        bkeys[byteLength - 1] = static_cast<uint8_t>(x);
        z = x & 0xff;
    }

    for (uint8_t i = 0; i < numKeys; ++i)
    {
        data[2 + i] ^= wkeys[i];
    }
}

OpenAce::PostConstruct Flarm2024::postConstruct()
{
    frameConsumerQueue = xQueueCreate(4, sizeof(OpenAce::RadioRxFrame));
    if (frameConsumerQueue == nullptr)
    {
        return OpenAce::PostConstruct::XQUEUE_ERROR;
    }
    return OpenAce::PostConstruct::OK;
}

void Flarm2024::start()
{
    xTaskCreate(flarmReceiveTask, "flarmReceiveTask", configMINIMAL_STACK_SIZE + 2048, this, tskIDLE_PRIORITY + 2, &taskHandle);
    getBus().subscribe(*this);
};

void Flarm2024::stop()
{
    getBus().unsubscribe(*this);
    vTaskDelete(taskHandle);
    vQueueDelete(frameConsumerQueue);
};

void Flarm2024::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    for (const auto &stat : dataSourceTimeStats)
    {
        stream << "\"f" << stat.frequency << "\":\"" << stat.timeTenthMs.to_string() << "\",";
    }
    stream << "\"receivedAircraftPositions\":" << statistics.receivedAircraftPositions;
    stream << ",\"transmittedAircraftPositions\":" << statistics.transmittedAircraftPositions;
    stream << ",\"crcErr\":" << statistics.crcErr;
    stream << ",\"outOfDistance\":" << statistics.outOfDistance;
    stream << ",\"ownshipAddress\":" << openAceConfiguration.address;
    stream << ",\"queueFullErr\":" << statistics.queueFullErr;
    stream << "}\n";
}

void Flarm2024::addReceiveStat(uint32_t frequency)
{
    // TODO: Something strange happening with multiple frequencies we receive
    auto msInSec = 99 - CoreUtils::msInSecond() / 10;
    for (auto &stat : dataSourceTimeStats)
    {
        if (stat.frequency == frequency)
        {
            stat.timeTenthMs.set(msInSec);
            return;
        }
    }

    if (!dataSourceTimeStats.full())
    {
        // If frequency not found, add a new stat
        dataSourceTimeStats.push_back(DataSourceTimeStats{});
        dataSourceTimeStats.back().frequency = frequency;
        dataSourceTimeStats.back().timeTenthMs.set(msInSec);
    }
}

int8_t Flarm2024::parseFrame(uint32_t *packet, uint32_t epochSeconds, int16_t rssiDbm)
{
    // dumpBuffer(msg.frame, msg.length);
    // Flarm messages are send well after PPS (400..1200ms after) so it would rarly
    // happen we are not using the correct TS and thus decryption failures
    RadioPacket *radioPacket = (RadioPacket *)packet;

#if !defined(UNIT_TESTING)

    bteaDecode(packet + 2);
    scramble(packet, epochSeconds);
    uint32_t packetCpy[sizeof(RadioPacket)];
    // Algorithm to find a possible epochTime where the received packed was scrambled at
    // It currently assumes that only when the epochSec & 0x0F rolls over there is a potential
    // This assumes the received packages is always send 'back' in time, echt current Epoch is always > epoch where
    // the packages was scrambled at.
    if (radioPacket->flarmTimestampLSB > ((uint8_t)(epochSeconds & 0x0F)))
    {
        constexpr int8_t timeOffsets[] = {-1, -2, 1};
        RadioPacket *radioPacket = (RadioPacket *)packetCpy;
        scramble(packet, epochSeconds);
        bteaDecode(packet + 2);

        for (auto offset : timeOffsets)
        {
            memcpy(packetCpy, packet, sizeof(RadioPacket));
            bteaDecode(packetCpy + 2);
            scramble(packetCpy, epochSeconds + offset);
            if (radioPacket->flarmTimestampLSB - ((uint8_t)((epochSeconds + offset) & 0x0F)) >= 14)
            {
                // printf(" Match found at %d", offset);
                break;
            }
        }
    }
#endif

    // If it's not 0x02, it's not position data
    if (radioPacket->messageType != 0x02)
    {
        return -2;
    }

    auto ownlat = ownshipPosition.lat;
    auto ownLon = ownshipPosition.lon;

    // Calculate rounded latitude
    int32_t ownLatInt = static_cast<int32_t>(ownlat * 1e7);
    int32_t round_lat = (ownLatInt + (ownLatInt < 0 ? -26 : 26)) / 52;

    // Adjust latitude
    int32_t ilat = (radioPacket->latitude - round_lat) & 0x0FFFFF;
    if (ilat >= 0x080000)
    {
        ilat -= 0x100000;
    }
    auto aircraftLat = static_cast<float>((ilat + round_lat) * 52) * 1e-7f;

    // Calculate rounded longitude
    int lonDiv = lonDivisor(aircraftLat);
    int32_t ownLonInt = static_cast<int32_t>(ownLon * 1e7);
    int32_t round_lon = (ownLonInt + (ownLonInt < 0 ? -(lonDiv >> 1) : (lonDiv >> 1))) / lonDiv;

    // Adjust longitude
    int32_t ilon = (radioPacket->longitude - round_lon) & 0x0FFFFF;
    if (ilon >= 0x080000)
    {
        ilon -= 0x0100000;
    }
    auto aircraftLon = static_cast<float>((ilon + round_lon) * lonDiv) * 1e-7f;

    auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownlat, ownLon, aircraftLat, aircraftLon);

    // printf("Distance from own: %ldm epoch:%ld flarmLSB:%d ownLSB:%d \n", fromOwn.distance, epochSeconds, radioPacket->flarmTimestampLSB, (uint8_t)(epochSeconds & 0x0F));
    if (fromOwn.distance > distanceIgnore)
    {
        statistics.outOfDistance++;
        return -1;
    }

    float groundSpeed = descale<8, 2, false>(radioPacket->groundSpeed) / 10.0f;

//    printf("FLARM: time:%d address:%06X latitude:%0.6f longitude:%0.6f altitude:%d climbRate:%0.2f speed:%0.2f heading:%d turnRate:%0.2f\n",
//       (uint16_t)(CoreUtils::msSinceEpoch() % 1000), radioPacket->aircraftID, aircraftLat, aircraftLon, descale<12, 1, false>(radioPacket->altitude) - 1000, descale<6, 2, true>(radioPacket->verticalSpeed) / 10.0f, groundSpeed, static_cast<int16_t>(radioPacket->course >> 1), descale<6, 2, true>(radioPacket->turnRate) / 20.0f);

    statistics.receivedAircraftPositions++;
    OpenAce::AircraftPositionMsg aircraftPosition{
        OpenAce::AircraftPositionInfo{
            CoreUtils::getPositionTs(),
            "",
            radioPacket->aircraftID,
            addressTypeFromFlarm(radioPacket->addressType),
            OpenAce::DataSource::FLARM,
            static_cast<OpenAce::AircraftCategory>(radioPacket->aircraftType),
            radioPacket->stealth,
            radioPacket->noTrack,
            groundSpeed > OpenAce::GROUNDSPEED_CONSIDERING_AIRBORN,
            aircraftLat,
            aircraftLon,
            descale<12, 1, false>(radioPacket->altitude) - 1000,
            descale<6, 2, true>(radioPacket->verticalSpeed) / 10.0f,
            groundSpeed,
            static_cast<int16_t>(radioPacket->course >> 1),
            descale<6, 2, true>(radioPacket->turnRate) / 20.0f,
            static_cast<uint16_t>(fromOwn.distance),
            fromOwn.relNorth,
            fromOwn.relEast,
            fromOwn.bearing},
        rssiDbm};

    getBus().receive(aircraftPosition);
    return 0;
}

void Flarm2024::flarmReceiveTask(void *arg)
{
    Flarm2024 *flarm = static_cast<Flarm2024 *>(arg);
    while (true)
    {
        OpenAce::RadioRxFrame msg;
        if (xQueueReceive(flarm->frameConsumerQueue, &msg, portMAX_DELAY) == pdPASS)
        {
            // Validate checksum
            RadioPacket *packet = (RadioPacket *)msg.frame;

            uint16_t calculatedChecksum = flarmCalculateChecksum((uint8_t *)msg.frame, RadioPacket::packetLength);
            if (calculatedChecksum != swapBytes16(packet->checksum))
            {
                flarm->statistics.crcErr++;
                continue;
            }

            // Ignore ownship address
            if (packet->aircraftID == flarm->openAceConfiguration.address)
            {
                continue;
            }

            flarm->addReceiveStat(msg.frequency);
            flarm->parseFrame(msg.frame, msg.epochSeconds, msg.rssidBm);
        }
    }
}

void Flarm2024::on_receive(const OpenAce::RadioRxFrame &msg)
{
    if (msg.dataSource == OpenAce::DataSource::FLARM)
    {
        const OpenAce::RadioRxFrame cpy = msg;
        if (xQueueSendToBack(frameConsumerQueue, &cpy, TASK_DELAY_MS(5)) != pdPASS)
        {
            statistics.queueFullErr++;
        }
    }
}

void Flarm2024::on_receive(const OpenAce::OwnshipPositionMsg &msg)
{
    ownshipPosition = msg.position;
}

void Flarm2024::on_receive(const OpenAce::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == "config")
    {
        openAceConfiguration = msg.config.openAceConfig();
    }
}

void Flarm2024::on_receive(const OpenAce::RadioTxPositionRequest &msg)
{
    if (msg.radioParameters.config.dataSource == OpenAce::DataSource::FLARM)
    {

        uint8_t addressType = static_cast<uint8_t>(openAceConfiguration.addressType);

        auto epochSeconds = CoreUtils::secondsSinceEpoch();
        auto ownLat = ownshipPosition.lat;
        auto ownLon = ownshipPosition.lon;

        uint32_t latitude, longitude;
        if (ownLat < 0.f)
        {
            latitude = (uint32_t)(-(((int32_t)(-ownLat * 1e7) + 26.f) / 52.f)) & 0x0FFFFF;
        }
        else
        {
            latitude = (uint32_t)((((uint32_t)(ownLat * 1e7) + 26.f) / 52.0f)) & 0x0FFFFF;
        }

        auto divisor = lonDivisor(ownLat);
        if (ownLon < 0.f)
        {
            longitude = (uint32_t)(-(((int32_t)(-ownLon * 1e7) + (divisor >> 1)) / divisor)) & 0x0FFFFF;
        }
        else
        {
            longitude = (((uint32_t)(ownLon * 1e7) + (divisor >> 1)) / divisor) & 0x0FFFFF;
        }

        Flarm2024::RadioPacket radioPacket =
            {
                .aircraftID = openAceConfiguration.address, // ownshipPosition.address,
                .messageType = 0x02,
                .addressType = addressType <= 2 ? addressType : uint8_t(0),
                .reserved1 = 0,
                .stealth = openAceConfiguration.stealth,
                .noTrack = openAceConfiguration.noTrack,
                .reserved3 = 0b11,
                .reserved4 = 0b11,
                .reserved5 = 0x00,
                .flarmTimestampLSB = static_cast<uint8_t>(epochSeconds & 0x0F),
                .aircraftType = static_cast<uint8_t>(openAceConfiguration.category),
                .reserved7 = 0x00,
                .altitude = static_cast<uint16_t>(enscale<12, 1, false>(ownshipPosition.altitudeWgs84 + 1000)),
                .latitude = latitude,
                .longitude = longitude,
                .turnRate = static_cast<uint16_t>(enscale<6, 2, true>(ownshipPosition.hTurnRate * 20)),
                .groundSpeed = static_cast<uint16_t>(enscale<8, 2, false>(ownshipPosition.groundSpeed * 10)),
                .verticalSpeed = static_cast<uint16_t>(enscale<6, 2, true>(ownshipPosition.verticalSpeed * 10)),
                .course = static_cast<uint16_t>(ownshipPosition.course * 2),
                .movementStatus = static_cast<uint8_t>(ownshipPosition.groundSpeed > OpenAce::GROUNDSPEED_CONSIDERING_AIRBORN ? 2 : 1), // TODO: Add support for Circling
                .gnssHorizontalAccuracy = 0b010010,                                                                                     // enscale<3, 2, false>((gpsStats.hDop*2+5)/10),
                .gnssVerticalAccuracy = 0b01010,                                                                                        // enscale<2, 3, false>((gpsStats.pDop*2+5)/10),
                .unknownData = 11,
                .reserved8 = 0,
                .checksum = 0}; // Will get calculated later.

        uint32_t *data = reinterpret_cast<uint32_t *>(&radioPacket);

#if !defined(UNIT_TESTING)
        scramble(data, epochSeconds);
        bteaEncode(data + 2);
        //btea2(data, true);
#endif

        uint16_t calculatedChecksum = flarmCalculateChecksum(reinterpret_cast<uint8_t *>(data), RadioPacket::packetLength);
        radioPacket.checksum = swapBytes16(calculatedChecksum);

        statistics.transmittedAircraftPositions++;

        getBus().receive(OpenAce::RadioTxFrame{
            Radio::TxPacket{
                msg.radioParameters,
                RadioPacket::totalLengthWCRC,
                data},
            msg.radioNo});
    }
}

OpenAce::AddressType Flarm2024::addressTypeFromFlarm(uint8_t addressType) const
{
    constexpr etl::array<OpenAce::AddressType, 3> lookupTable =
        {
            OpenAce::AddressType::RANDOM, // 0x00
            OpenAce::AddressType::ICAO,   // 0x01
            OpenAce::AddressType::FLARM,  // 0x02
        };
    if (addressType > 2)
        return OpenAce::AddressType::RANDOM;
    return lookupTable[addressType];
}

uint8_t Flarm2024::addressTypeToFlarm(OpenAce::AddressType addressType) const
{
    switch (addressType)
    {
    case OpenAce::AddressType::ICAO:
        return 0x01;
    case OpenAce::AddressType::FLARM:
        return 0x02;
    default:
        return 0;
    }
}
