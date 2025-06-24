#include <stdio.h>

#include "ogn1.hpp"
#include "ognpacket.hpp"
#include "ace/bitcount.hpp"
#include "etl/algorithm.h"


constexpr float POSITION_DECODE = 0.0001f / 60.f;
constexpr float POSITION_ENDECODE = 1.f / POSITION_DECODE;

LDPC_Decoder<Ogn1::OGN_PACKET_LENGTH*8, 48> Ogn1::decoder; // 1248 bytes, should be 

GATAS::PostConstruct Ogn1::postConstruct()
{
    if (sizeof(OGN1_Packet) != OGN_PACKET_LENGTH_FEC + 2) // 20byte + FEC == 6 byte + 2 extra for the word that is ignored
    {
        panic("OGN1 packet is smaller than expected");
        return GATAS::PostConstruct::FAILED;
    }

    return GATAS::PostConstruct::OK;
}

void Ogn1::start()
{
    getBus().subscribe(*this);
};

void Ogn1::stop()
{
    getBus().unsubscribe(*this);
};

void Ogn1::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    for (const auto &stat : dataSourceTimeStats)
    {
        stream << "\"f" << stat.frequency << "\":\"" << stat.timeTenthMs.to_string() << "\",";
    }

    stream << "\"relay\":[";
    for (uint8_t idx = 0; idx < 4; idx++)
    {
        stream << statistics.relay[idx];
        if (idx < 3)
        {
            stream << ","; // Add a comma between elements
        }
    }
    stream << "],";

    stream << "\"receivedAircraftPositions\":" << statistics.receivedAircraftPositions;
    stream << ",\"transmittedAircraftPositions\":" << statistics.transmittedAircraftPositions;
    stream << ",\"fecErr\":" << statistics.fecErr;
    stream << ",\"outOfDistance\":" << statistics.outOfDistance;
    stream << ",\"encrypted\":" << statistics.encrypted;
    stream << ",\"nonPositional\":" << statistics.nonPositional;
    stream << "}\n";
}

void Ogn1::addReceiveStat(uint32_t frequency)
{
    auto msInSec = 99 - CoreUtils::msInSecond() / 10;
    for (auto &stat : dataSourceTimeStats)
    {
        if (stat.frequency == frequency)
        {
            stat.timeTenthMs.set(msInSec);
            return;
        }
    }

    // If frequency not found, add a new stat
    if (!dataSourceTimeStats.full())
    {
        // If frequency not found, add a new stat
        dataSourceTimeStats.push_back(DataSourceTimeStats{});
        dataSourceTimeStats.back().frequency = frequency;
        dataSourceTimeStats.back().timeTenthMs.set(msInSec);
    }
}

uint8_t Ogn1::ErrCount(const uint8_t *err, uint8_t length) // count detected manchester errors
{
    uint8_t count = 0;
    for (uint8_t idx = 0; idx < length; idx++)
    {
        count += Count1s(err[idx]);
    }
    return count;
}

uint8_t Ogn1::ErrCount(const uint8_t *output, const uint8_t *data, const uint8_t *err, uint8_t length)  // count errors compared to data corrected by FEC
{
    uint8_t count = 0;
    for (uint8_t idx = 0; idx < length; idx++)
    {
        count += Count1s((uint8_t)((data[idx] ^ output[idx]) & (~err[idx])));
    }
    return count;
}

uint8_t Ogn1::errorCorrect(uint8_t *output, uint8_t *data, uint8_t *err, uint8_t iter)
{

    uint8_t check = 0;
    uint8_t errCount = ErrCount(err, OGN_PACKET_LENGTH); // conunt Manchester decoding errors
    Ogn1::decoder.Input(data, err);                            // put data into the FEC decoder
    do                                                   // more loops is more chance to recover the packet
    {
        check = decoder.ProcessChecks(); // do an iteration
    } while ((iter--) && check); // if FEC all fine: break
    Ogn1::decoder.Output(output); // get corrected bytes into the OGN packet
    errCount += ErrCount(output, data, err, OGN_PACKET_LENGTH);

    errCount = etl::min(errCount, (uint8_t)15);
    check = etl::min(check, (uint8_t)15);
    return (check & 0x0F) | (errCount << 4);
}

GATAS::AddressType Ogn1::addressTypeFromOgn(uint8_t addressType) const
{
    switch (addressType)
    {
    case 0x03:
        return GATAS::AddressType::OGN;
    case 0x02:
        return GATAS::AddressType::FLARM;
    case 0x01:
        return GATAS::AddressType::ICAO;
    default:
        return GATAS::AddressType::RANDOM;
    }
}

uint8_t Ogn1::addressTypeToOgn(GATAS::AddressType addressType) const
{
    constexpr uint8_t lookupTable[] =
        {
            0x00, // RANDOM
            0x01, // ICAO
            0x02, // FLARM
            0x03, // OGN (default to 0x00, update if needed)
            0x00, // FANET (default to 0x00, update if needed)
            0x00, // ADSL (default to 0x00, update if needed)
            0x00, // RESERVED (default to 0x00, update if needed)
            0x00  // UNKNOWN (default to 0x00, update if needed)
        };

    uint8_t index = static_cast<uint8_t>(addressType) & 0x07;
    return lookupTable[index];
}

int8_t Ogn1::parseFrame(OGN1_Packet &packet, int16_t rssiDbm)
{
    uint32_t timeUs32 = CoreUtils::timeUs32();
    if (packet.Header.NonPos)
    {
        statistics.nonPositional++;
        return 0;
    }
    statistics.relay[packet.Header.Relay % 4]++;

    float fLatitude = POSITION_DECODE * packet.DecodeLatitude();
    float fLongitude = POSITION_DECODE * packet.DecodeLongitude();

    auto ownship = ownshipPosition.load(etl::memory_order_acquire);

    auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownship.lat, ownship.lon, fLatitude, fLongitude);

//    printf("OGN: address:%06X latitude:%0.6f longitude:%0.6f altitude:%ld offset:%d, stdaltitude:%ld climbRate:%d speed:%d heading:%0.2f turnRate:%0.2f\n",
//       packet.Header.Address, fLatitude, fLongitude, packet.DecodeAltitude() * 10 + ownshipPosition.geoidOffset, ownshipPosition.geoidOffset, packet.DecodeStdAltitude(), packet.DecodeClimbRate(), packet.DecodeSpeed(), packet.DecodeHeading() * 0.1f, packet.DecodeTurnRate()*0.1f);

    if (fromOwn.distance > distanceIgnore)
    {
        statistics.outOfDistance++;
        // printf("Distance %2.f > GATAS_FLARM_IGNORE_DISTANCE_ERRORS, ignoring (%.4f, %.4f, %.4f, %.4f)\n",   distance, fLatitude, fLongitude, lastGpsPosition.latitude, lastGpsPosition.longitude);
        return -1;
    }

    statistics.receivedAircraftPositions++;
    int16_t speed0d1ms = packet.DecodeSpeed();

    GATAS::AircraftPositionMsg aircraftPosition{
        GATAS::AircraftPositionInfo{
            timeUs32,
            "",
            packet.Header.Address,
            addressTypeFromOgn(packet.Header.AddrType),
            GATAS::DataSource::OGN1,
            static_cast<GATAS::AircraftCategory>(packet.Position.AcftType),
            packet.Position.Stealth ? true : false, // Privacy
            gaTasConfiguration.noTrack,           // noTrack
            1,                                      // airBorn
            fLatitude,
            fLongitude,
            packet.DecodeAltitude() + CoreUtils::egmGeoidOffset(fLatitude, fLongitude),   // OGN Sends MSL
            packet.DecodeClimbRate() * .1f,         // Climb Rate is send s 0.1m/s (10 means 1/ms)
            speed0d1ms * .1f,
            static_cast<int16_t>(packet.DecodeHeading() * .1f),
            packet.DecodeTurnRate() * .1f,
            fromOwn.distance,
            fromOwn.relNorth,
            fromOwn.relEast,
            fromOwn.bearing},
        rssiDbm};
    getBus().receive(aircraftPosition);
    return 0;
}

void Ogn1::on_receive(const GATAS::RadioTxPositionRequestMsg &msg)
{
    if (msg.radioParameters.config.dataSource == GATAS::DataSource::OGN1)
    {
        OGN1_Packet packet;
        packet.Header =
            {
                .Address = gaTasConfiguration.address, // Address
                .AddrType = addressTypeToOgn(gaTasConfiguration.addressType),
                .NonPos = 0,    // 0 = position packet, 1 = other information like status
                .Parity = 0,    // parity takes into account bits 0..27 thus only the 28 lowest bits
                .Relay = 0,     // 0 = direct packet, 1 = relayed once, 2 = relayed twice, ...
                .Encrypted = 0, // packet is encrypted
                .Emergency = 0, // aircraft in emergency (not used for now)
            };

        packet.calcAddrParity();

        auto ownship = ownshipPosition.load(etl::memory_order_acquire);
        packet.EncodeLatitude(ownship.lat * POSITION_ENDECODE);
        packet.EncodeLongitude(ownship.lon * POSITION_ENDECODE);
        packet.EncodeSpeed(ownship.groundSpeed * 10.f);
        packet.EncodeHeading(ownship.course * 10.f);
        packet.EncodeClimbRate(ownship.verticalSpeed * 10.f);
        packet.EncodeTurnRate(ownship.hTurnRate * 10.f);
        packet.EncodeAltitude(ownship.altitudeHAE);              
        packet.EncodeDOP(gpsStats.pDop + 0.5f);

        // TODO: Understand how baro Altitude really works in OGN
        packet.clrBaro();
        // if (CoreUtils::msElapsed(lastBarometricPressureMsg.msSinceBoot) > 4'000)
        // {
        //     packet.clrBaro();
        // }
        // else
        // {
        //     packet.EncodeStdAltitude(lastBarometricPressureMsg.pressurehPa);
        // }

        packet.Position.FixQuality = gpsStats.fixQuality < 3 ? gpsStats.fixQuality : 0;
        packet.Position.FixMode = packet.Position.FixQuality > 0 ? gpsStats.fixType : 0;

        auto msSinceEpoch = CoreUtils::msSinceEpoch();
        tm time = CoreUtils::localTime(msSinceEpoch);
        uint8_t secondTime = time.tm_sec;
        // Round time to nearest full second
        if (CoreUtils::msInSecond() >= 500)
        {
            secondTime++;
            if (secondTime >= 60)
            {
                secondTime -= 60;
            }
        }
        packet.Position.Time = secondTime;

        packet.Whiten();
        LDPC_Encode(packet.Word());
        statistics.transmittedAircraftPositions++;

        getBus().receive(GATAS::RadioTxFrameMsg{
            Radio::TxPacket{
                msg.radioParameters,
                OGN_PACKET_LENGTH_FEC,
                &packet},
            msg.radioNo});
        // printf("OGN request position\n");
    }
}

void Ogn1::on_receive(const GATAS::RadioRxGfskMsg &msg)
{
    if (msg.dataSource == GATAS::DataSource::OGN1)
    {
        OGN1_Packet packet;
        // Validate packet, and correct if possible
        uint8_t check = Ogn1::errorCorrect((uint8_t *)&packet, (uint8_t *)msg.frame, (uint8_t *)msg.err);
        if (check & 0x0F)
        {
            statistics.fecErr++;
            return;
        }
        // dumpBuffer((uint8_t*)msg.frame, msg.length);
        packet.Dewhiten();
        if (packet.Header.Encrypted)
        {
            statistics.encrypted++;
            return;
        }

        // Ignore ownship address
        if (packet.Header.Address == gaTasConfiguration.address) {
            return;
        }

        addReceiveStat(msg.frequency);
        parseFrame(packet, msg.rssidBm);
    }
}

void Ogn1::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    ownshipPosition.store(msg.position, etl::memory_order_release);
}

void Ogn1::on_receive(const GATAS::BarometricPressureMsg &msg)
{
    lastBarometricPressureMsg = msg;
}

void Ogn1::on_receive(const GATAS::GpsStatsMsg &msg)
{
    gpsStats = msg;
}

void Ogn1::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::CONFIG)
    {
        gaTasConfiguration = msg.config.gaTasConfig();
    }
}

void Ogn1::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}