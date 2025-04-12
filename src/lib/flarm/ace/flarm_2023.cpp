#include <stdio.h>

#include <math.h>
#include "flarm_common.hpp"



uint32_t flarmObscure(uint32_t key, uint32_t seed)
{
    uint32_t m1 = seed * (key ^ (key >> 16));
    uint32_t m2 = (seed * (m1 ^ (m1 >> 16)));
    return m2 ^ (m2 >> 16);
}

/**
 * Seems like we get a new key every 64s timestamp >> 6
*/
void flarmMakekey(uint32_t key[4], uint32_t epochSeconds, uint32_t address)
{
    uint32_t keys[] = {0xe43276df, 0xdca83759, 0x9802b8ac, 0x4675a56b,
                       0xfc78ea65, 0x804b90ea, 0xb76542cd, 0x329dfa32
                      };
    for (uint8_t i = 0; i < 4; i++)
    {
        uint8_t ndx = ((epochSeconds >> 23) & 1) ? i+4 : i ;
        key[i] = flarmObscure(keys[ndx] ^ ((epochSeconds >> 6) ^ address), 0x045d9f3b) ^ 0x87b562f4;
    }
}

void flarmEncrypt(uint32_t *flarm_pkt, uint32_t epochSeconds)
{
    flarmV7Packet_t *pkt = (flarmV7Packet_t *) flarm_pkt;
    uint32_t key[4];
    flarmMakekey(key, epochSeconds, (pkt->address << 8) & 0xffffff);
    xxteaEncrypt(&flarm_pkt[1], 5, key, 6);
}

void flarmDecrypt(uint32_t *flarm_pkt, uint32_t epochSeconds)
{
    flarmV7Packet_t *pkt = (flarmV7Packet_t *) flarm_pkt;
    uint32_t key[4];
    flarmMakekey(key, epochSeconds, (pkt->address << 8) & 0xffffff);
    xxteaDecrypt(&flarm_pkt[1], 5, key, 6); // adrress= 3byte + magic + 20 bytes (5 words) + 2byte checksum
}

uint16_t flarmCalculateChecksum(uint8_t* flarm_pkt, uint8_t length)
{
    uint16_t crc16 = 0xffff;
    // Add the Flarm address that is not in the packet see:CountryRegulations
    crc16 = update_crc_ccitt(crc16, 0x31);
    crc16 = update_crc_ccitt(crc16, 0xFA);
    crc16 = update_crc_ccitt(crc16, 0xB6);

    for (uint8_t i =0; i<length; i++)
        crc16 = update_crc_ccitt(crc16, (uint8_t)(flarm_pkt[i]));

    return crc16;
}



OpenAce::PostConstruct Flarm::postConstruct()
{
    BaseModule::moduleByName(*this, Tuner::NAME);
    if (sizeof(flarmV7Packet_t) != 24)
    {
        panic("Flarm packet size is not 24 bytes");
        return OpenAce::PostConstruct::FAILED;
    }

    frameConsumerQueue = xQueueCreate( 4, sizeof( OpenAce::RadioRxGfskMsg ) );
    if (frameConsumerQueue == nullptr)
    {
        panic("Failed to create frameConsumerQueue");
        return OpenAce::PostConstruct::FAILED;
    }
    return OpenAce::PostConstruct::OK;
}

void Flarm::start()
{
    xTaskCreate(flarmReceiveTask, "flarmReceiveTask", configMINIMAL_STACK_SIZE+64, this, tskIDLE_PRIORITY + 2, &taskHandle);
//    getBus().receive(OpenAce::DataSourceListen{OpenAce::DataSource::FLARM, true});
    // auto tuner = static_cast<Tuner*>(BaseModule::moduleByName(*this, Tuner::NAME));
    // tuner->startListen(OpenAce::DataSource::FLARM);
    getBus().subscribe(*this);
};

void Flarm::stop()
{
    getBus().unsubscribe(*this);
//    getBus().receive(OpenAce::DataSourceListen{OpenAce::DataSource::FLARM, false});
    // auto tuner = static_cast<Tuner*>(BaseModule::moduleByName(*this, Tuner::NAME));
    // tuner->stopListen(OpenAce::DataSource::FLARM);

    vTaskDelete(taskHandle);
    vQueueDelete(frameConsumerQueue);
};

void Flarm::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    for(const auto &stat : dataSourceTimeStats)
    {
        stream << "\"f" << stat.frequency << "\":\"" << stat.timeTenthMs.to_string() << "\",\n";
    }

    stream << "\"receivedAircraftPositions\":" << statistics.receivedAircraftPositions;
    stream << ",\"transmittedAircraftPositions\":" << statistics.transmittedAircraftPositions;
    stream << ",\"crcErrors\":" << statistics.crcErrors;
    stream << ",\"outOfDistance\":" << statistics.outOfDistance;
    stream << ",\"zero0x01Err\":" << statistics.zero0x01Err;
    stream << ",\"zero0Err\":" << statistics.zero0Err;
    stream << ",\"addressTypeErr\":" << statistics.addressTypeErr;
    stream << ",\"addressTypeFlarm\":" << statistics.addressTypeFlarm;
    stream << ",\"addressTypeRandom\":" << statistics.addressTypeRandom;
    stream << ",\"addressTypeICAO\":" << statistics.addressTypeICAO;
    stream << ",\"queueFull\":" << statistics.queueFull;
    stream << "}\n";
}


void Flarm::addReceiveStat(uint32_t frequency)
{
    auto msInSec = 99-CoreUtils::msInSecond() / 10;
    for (auto &stat : dataSourceTimeStats)
    {
        if (stat.frequency == frequency)
        {
            stat.timeTenthMs.set(msInSec);
            return;
        }
    }

    // If frequency not found, add a new stat
    dataSourceTimeStats.push_back(DataSourceTimeStats{});
    dataSourceTimeStats.back().frequency = frequency;
    dataSourceTimeStats.back().timeTenthMs.set(msInSec);
}

int8_t Flarm::parseFrame(uint32_t *packet, uint32_t epochSeconds, int16_t rssiDbm)
{
    // dumpBuffer(msg.frame, msg.length);
    // Flarm messages are send well after PPS (400..1200ms after) so it would rarly
    // happen we are not using the correct TS and thus decryption failures
#if !defined(UNIT_TESTING)
    flarmDecrypt(packet, epochSeconds);
#endif
    flarmV7Packet_t *flarmPacket = (flarmV7Packet_t *)packet;

    uint32_t positionTs = CoreUtils::timeUs32();
    // When this bit is zero, it's position data
    if ((flarmPacket->zero0 & 0x01) == 0x01)
    {
        statistics.zero0x01Err++;
        return -2;
    }
    if (flarmPacket->zero0 != 0x00)
    {
        statistics.zero0Err++;
        return -3;
    }

    int32_t round_lat = ((int32_t) (ownshipPosition.lat * 1e7)) >> 7;
    int32_t latitude = (flarmPacket->latitude - round_lat) % (uint32_t) 0x080000;
    if (latitude >= 0x040000) latitude -= 0x080000;
    float fLatitude = ((latitude + round_lat) << 7) / 1e7;

    int32_t round_lon = ((int32_t) (ownshipPosition.lon * 1e7)) >> 7;
    int32_t longitude = (flarmPacket->longitude - round_lon) % (uint32_t) 0x100000;
    if (longitude >= 0x080000) longitude -= 0x100000;
    float fLongitude = ((longitude + round_lon) << 7) / 1e7;

    auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt( ownshipPosition.lat, ownshipPosition.lon, fLatitude, fLongitude);
    if (fromOwn.distance > distanceIgnore)
    {
        statistics.outOfDistance++;
        // printf("Distance %2.f > OPENACE_FLARM_IGNORE_DISTANCE_ERRORS, ignoring (%.4f, %.4f, %.4f, %.4f)\n",   distance, fLatitude, fLongitude, lastGpsPosition.latitude, lastGpsPosition.longitude);
        return -1;
    }

    // Move the sign bit to the right place for verticalSpeed
    int16_t vsTwosCompl = (int16_t) (flarmPacket->verticalSpeed | (flarmPacket->verticalSpeed & 0b1000000000 ? 0b1111110000000000 : 0));
    int16_t northSouth = ((int16_t)flarmPacket->ns[0] + (int16_t)flarmPacket->ns[1] + (int16_t)flarmPacket->ns[2] + (int16_t)flarmPacket->ns[3]) / 4;
    int16_t easyWest = ((int16_t)flarmPacket->ew[0] + (int16_t)flarmPacket->ew[1] + (int16_t)flarmPacket->ew[2] + (int16_t)flarmPacket->ew[3]) / 4;
    float speedM4S = sqrtf(northSouth * northSouth + easyWest * easyWest) * (1 << flarmPacket->speedscale); // Speed in meters per 4 seconds

    float track;
    bool noTrack;
    if (speedM4S > 0)
    {
        track = atan2f(northSouth, easyWest) * RADS_TO_DEG;
        track = (track <= 90.f ? 90.f - track : 450.f - track);
        noTrack = false;
    }
    else
    {
        track = 0;
        noTrack = true;
    }

    statistics.receivedAircraftPositions++;

    OpenAce::AircraftPositionMsg aircraftPosition
    {
        OpenAce::AircraftPositionInfo
        {
            positionTs,
            flarmPacket->address,
            addressType(flarmPacket->addressType),
            OpenAce::DataSource::FLARM,
            static_cast<OpenAce::AircraftCategory>(flarmPacket->aircraftType),   // We can cast this because AircraftCategory follows FLARM spec
            flarmPacket->stealth,                         // stealth somehow relates to random address, but different :)
            noTrack,                                 //
            flarmPacket->turnRate != TURN_RATE_ON_GROUND, // Airborn
            fLatitude,
            fLongitude,
            static_cast<int16_t>(flarmPacket->altitude),  // relative to WGS84 ellipsoid
            (vsTwosCompl << flarmPacket->speedscale) * DPMS_TO_MS,
            speedM4S / 4.0f,                         // Speed was send as meters per 4 seconds/ scale back to m/s
            static_cast<int16_t>(track),
            0.0f,
            fromOwn.distance,
            fromOwn.relNorth,
            fromOwn.relEast,
            fromOwn.bearing
        },
        rssiDbm
    };
    getBus().receive(aircraftPosition );
    return 0;
}


void Flarm::flarmReceiveTask(void *arg)
{
    Flarm *flarm = static_cast<Flarm*>(arg);
    while (true)
    {
        OpenAce::RadioRxGfskMsg msg;
        if (xQueueReceive(flarm->frameConsumerQueue, &msg, portMAX_DELAY) == pdPASS)
        {
            // Validate checksum
            flarmV7Packet_t *packet = (flarmV7Packet_t *)msg.frame;
            uint16_t calculatedChecksum = flarmCalculateChecksum((uint8_t*)msg.frame, flarmV7Packet_t::packetLength);
            if (calculatedChecksum != swapBytes16(packet->checksum))
            {
                flarm->statistics.crcErrors++;
                continue;
            }

            flarm->addReceiveStat(msg.frequency);
            flarm->parseFrame(msg.frame, msg.timestamp, msg.rssidBm);
        }
    }
}

const flarmV7Packet_t Flarm::on_receive(const OpenAce::RadioTxPositionRequestMsg &msg)
{
    flarmV7Packet_t packet;
    if (msg.radioParameters.config.dataSource == OpenAce::DataSource::FLARM)
    {
        auto speedScale = getSpeedScaling();

        int32_t lat_deg_e7 = ownshipPosition.lat * 1e7;
        int32_t lon_deg_e7 = ownshipPosition.lon * 1e7;

        uint32_t lat128 = lat_deg_e7 >> 7;
        // When added, the math is about 1m less accurate om some cases
        // if (lat128 & 0x40)
        // {
        //     lat128 += 1;
        // }
        lat128 &= 0x7ffff;


        uint32_t lon128 = lon_deg_e7 >> 7;
        // When added, the math is about 1m less accurate om some cases
        // if (lon128 & 0x40)
        // {
        //     lon128 += 1;
        // }
        lon128 &= 0x0FFFFF;

        // FLarm does not do OGN so we map that to RANDOM
        uint8_t addressType = static_cast<uint8_t>(ownshipPosition.addressType);
        if (addressType>2) addressType=0;

        packet.address = ownshipPosition.address;
        packet.zero0 = 0;
        packet.addressType = addressType;
        packet.verticalSpeed = ((uint16_t)(roundf(ownshipPosition.verticalSpeed * MS_TO_DMPS))) >> speedScale;
        packet.speedscale = speedScale;
        packet.turnRate = getTurnState();
        packet.stealth = ownshipPosition.stealth;
        packet.noTrack = ownshipPosition.noTrack;
        packet.vacc_m = 0; // TODO: How to get these?
        packet.hacc_m = 0; // TODO: How to get these?
        packet.aircraftType = static_cast<uint8_t>(ownshipPosition.aircraftType);
        packet.latitude = lat128;
        packet.longitude = lon128;
        //https://www.flarm.com/support/faq/  For collision avoidance with other FLARM equipped aircraft, FLARM uses only the more accurate GPS altitude difference.
        packet.altitude = ownshipPosition.altitudeWgs84<0?0:(ownshipPosition.altitudeWgs84>0x1fff?0x1fff:ownshipPosition.altitudeWgs84); // Is this referenced to Wgs84??
        packet.zero1 = 0;

        extrapolatVelocityVector(ownshipPosition.hTurnRate, ownshipPosition.velocityNorth, ownshipPosition.velocityEast, speedScale, packet.ns, packet.ew);
        packet.parity = buffersParity8((uint8_t*)&packet, sizeof(flarmV7Packet_t)-4);

#if !defined(UNIT_TESTING)
        flarmEncrypt(reinterpret_cast<uint32_t*>(&packet), CoreUtils::secondsSinceEpoch());
#endif

        uint16_t calculatedChecksum = flarmCalculateChecksum((uint8_t*)&packet, flarmV7Packet_t::packetLength);
        packet.checksum = swapBytes16(calculatedChecksum);

        statistics.transmittedAircraftPositions++;
        getBus().receive(OpenAce::RadioTxFrameMsg
        {
            Radio::TxPacket{
                msg.radioParameters,
                packet.totalLength,
                &packet
            },
            msg.radioNo
        });

        // printf("FLARM 6 request position\n");
    }
    return packet;
}

OpenAce::AddressType Flarm::addressType(uint8_t addressType)
{
    switch (addressType)
    {
    case 0x02:
        statistics.addressTypeFlarm++;
        return OpenAce::AddressType::FLARM;
    case 0x01:
        statistics.addressTypeICAO++;
        return OpenAce::AddressType::ICAO;
    case 0x00:
        statistics.addressTypeRandom++;
    default:
        // Not a bug, if we don't the the type we just say random
        statistics.addressTypeErr++;
        return OpenAce::AddressType::RANDOM;
    }
}

uint8_t Flarm::getTurnState() const
{

    // enum GxAirCOm
    // {
    // 	TURN_RATE_0,
    // 	TURN_RATE_ON_GROUND, //1 (plane on ground)
    // 	TURN_RATE_2,
    // 	TURN_RATE_3,
    // 	TURN_RATE_RIGHT_TURN, //4 (right turn >14deg)
    // 	TURN_RATE_NO_TURN, //5 (no/slow turn)
    // 	TURN_RATE_6,
    // 	TURN_RATE_LEFT_TURN //7 (left turn >14deg)
    // };

    if (!ownshipPosition.airborne)
    {
        return 1;
    }

    // No turn if this is not a glider or if moving faster than 36m/s.
    if (ownshipPosition.aircraftType != OpenAce::AircraftCategory::GliderMotorGlider || ownshipPosition.groundSpeed > 36.f)
    {
        return 5;
    }

    // The aircraft is a glider, check if it's making a turn of 14 degrees or more.
    if (ownshipPosition.hTurnRate > -14.f && ownshipPosition.hTurnRate < 14.f)
    {
        return 5;
    }

    // 4 for right turn, 7 for left turn
    return ownshipPosition.hTurnRate < 0 ? 7 : 4;
}

uint8_t Flarm::getSpeedScaling() const
{
    return getSpeedScaling(ownshipPosition.groundSpeed, ownshipPosition.verticalSpeed);
}

uint8_t Flarm::getSpeedScaling(float gspeed, float velU) const
{
    if (velU < 24.f && gspeed < 30.f)
    {
        // < 86.4 km/h, < 108 km/h
        return 0;
    }
    else if (velU < 48.f && gspeed < 62.f)
    {
        // < 172.8 km/h, < 223.2 km/h
        return 1;
    }
    else if (velU < 100.f && gspeed < 124.f)
    {
        // < 360.0 km/h, < 446.4 km/h
        return 2;
    }
    else
    {
        return 3;
    }
}


void Flarm::extrapolatVelocityVector(float deltaHeadingRad, float velNorthMS, float velEastMS, uint8_t speedScale, int8_t  velN[4], int8_t  velE[4] ) const
{
    constexpr uint8_t VEL_XPL_CNT = 18;
    etl::array<float, VEL_XPL_CNT> vel_n_i;
    etl::array<float, VEL_XPL_CNT> vel_e_i;

    float dhSin = sinf(deltaHeadingRad);
    float dhCos = cosf(deltaHeadingRad);

    float curVelN = velNorthMS;
    float curVelE = velEastMS;

    for (uint8_t i = 0; i < VEL_XPL_CNT; ++i)
    {
        float newVelN = curVelN * dhCos - curVelE * dhSin;
        float newVelE = curVelN * dhSin + curVelE * dhCos;

        vel_n_i[i] = newVelN;
        vel_e_i[i] = newVelE;

        curVelN = newVelN;
        curVelE = newVelE;
    }

    int ix = 1;
    for (uint8_t i = 0; i < 4; ++i)
    {
        float avgVelN = 0;
        float avgVelE = 0;
        for (uint8_t j = 0; j < 4; ++j)
        {
            avgVelN += vel_n_i[ix] / 2; // summing 4x cm/s gives cm/4s, /2
            avgVelE += vel_e_i[ix] / 2;
            ++ix;
        }

        avgVelN *= 2.0f;
        avgVelE *= 2.0f;
        for (uint8_t j = 0; j < speedScale; ++j)
        {
            avgVelN /= 2.0f;
            avgVelE /= 2.0f;
        }

        velN[i] = avgVelN;
        velE[i] = avgVelE;
    }
}
