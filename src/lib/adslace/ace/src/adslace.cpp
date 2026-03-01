#include <stdio.h>

#include "../adslace.hpp"
#include "ace/bitutils.hpp"
#include "ace/spinlockguard.hpp"
#include "adsl/adsl.hpp"
#include "ace/moreutils.hpp"
#include "ace/poolallocator.hpp"
#include "ace/semaphoreguard.hpp"

constexpr float POSITION_DECODE = 0.0001f / 60.f;

GATAS::PostConstruct ADSLAce::postConstruct()
{
    rxMutex = xSemaphoreCreateMutex();
    if (rxMutex == nullptr)
    {
        return GATAS::PostConstruct::MUTEX_ERROR;
    }

    return GATAS::PostConstruct::OK;
}

void ADSLAce::start()
{
    getBus().subscribe(*this);
};

void ADSLAce::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    for (const auto &stat : datasourceTimeStats.span())
    {
        stream << "\"f" << stat.frequency << "\":\"" << stat.timeTenthMs.to_string() << "\",\n";
    }
    stream << "\"receivedAircraftPositions\":" << statistics.receivedAircraftPositions;
    stream << ",\"transmittedAircraftPositions\":" << statistics.transmittedAircraftPositions;
    stream << ",\"fecErr\":" << statistics.fecErr;
    stream << ",\"outOfDistance\":" << statistics.outOfDistance;
    stream << ",\"encrypted\":" << statistics.encrypted;
    stream << ",\"protocolVersionErr\":" << statistics.protocolVersionErr;
    stream << ",\"decryptionKeyErr\":" << statistics.decryptionKeyErr;
    stream << ",\"payloadUnSupportedErr\":" << statistics.payloadUnSupportedErr;
    stream << ",\"unknownErr\":" << statistics.unknownErr;
    stream << ",\"unsupportedFec\":" << statistics.encrypted;
    stream << ",\"TXProtocolVersion\":" << protocol.lowestDominotorProtocolVersion();
    stream << ",\"numberOfNeighbours\":" << protocol.numberOfNeighbours();
    stream << "}";
}

void ADSLAce::on_receive(const GATAS::RadioRxMsg &msg)
{
    if (msg.dataSource == GATAS::DataSource::ADSLO_HDR)
    {
        PoolReleaseGuard frameGuard{getGlobalPool(), msg.frame};

        protocol.crcCheckOnReceive = true;
        auto status = protocol.handleRx(msg.rssidBm, msg.frame32Span()); // Currently expected that the frame size is always a multiple of 4

        // clang-format off
        switch (status)
        {
            case ADSL::Protocol::RxStatudeCode::OK: { 
                datasourceTimeStats.addReceiveStat(msg.frequency, CoreUtils::msInSecond());
                break;
            }
            case ADSL::Protocol::RxStatudeCode::CRC_FAILED:                   { statistics.fecErr += 1; break;}
            case ADSL::Protocol::RxStatudeCode::UNSUPORTED_PROTOCOL_VERSION:  { statistics.protocolVersionErr += 1; break; }
            case ADSL::Protocol::RxStatudeCode::UNSUPORTED_DECRYPTION_KEY:    { statistics.decryptionKeyErr += 1; break; }
            case ADSL::Protocol::RxStatudeCode::UNSUPORTED_ERROR_CONTROL_FEC: { statistics.unsupportedFec += 1; break; }
            case ADSL::Protocol::RxStatudeCode::UNSUPPORTED_PAYLOAD:          { statistics.payloadUnSupportedErr += 1; break; }
            default:                                                          { statistics.unknownErr += 1; break;}
        };
        // clang-format on
    }
}

void ADSLAce::on_receive(const GATAS::RadioRxManchesterMsg &msg)
{
    if (msg.dataSource != GATAS::DataSource::ADSLM && msg.dataSource != GATAS::DataSource::ADSLO_HDR)
    {
        return;
    }
    PoolReleaseGuard frameGuard{getGlobalPool(), msg.frame};
    PoolReleaseGuard errorGuard{getGlobalPool(), msg.error};

    // Two receivers can send a message at the same time, so protect the protocol handler with a mutex
    if (auto guard = SemaphoreGuard(20, rxMutex))
    {
        ADSL::Protocol::RxStatudeCode status = ADSL::Protocol::RxStatudeCode::OTHER_ERROR;
        if (msg.dataSource == GATAS::DataSource::ADSLM)
        {
            const size_t length = msg.frame[0];
            if (length > msg.lengthBytes || length < 16) // 16 Seems to be minimum length of a valid ADSL frame
            {
                status = ADSL::Protocol::RxStatudeCode::OTHER_ERROR;
            }
            else
            {
                // Remove length header (shift left)
                etl::mem_move(&msg.frame[1], length, msg.frame);
                etl::mem_move(&msg.error[1], length, msg.error);
                msg.lengthBytes = length;

                const int check = ADSL::Correct(msg.frameSpan(), msg.errorSpan());
                if (check == -1)
                {
                    status = ADSL::Protocol::RxStatudeCode::CRC_FAILED;
                }
                else
                {
                    protocol.crcCheckOnReceive = false;
                    status = protocol.handleRx(msg.rssidBm, msg.frame32Span());
                    datasourceTimeStats.addReceiveStat(msg.frequency, CoreUtils::msInSecond());
                }
            }
        }
        else // ADSLO_HDR
        {
            const int check = ADSL::Correct(msg.frameSpan(), msg.errorSpan());
            if (check == -1)
            {
                status = ADSL::Protocol::RxStatudeCode::CRC_FAILED;
            }
            else
            {
                protocol.crcCheckOnReceive = false;
                status = protocol.handleRx(msg.rssidBm, msg.frame32Span());
                datasourceTimeStats.addReceiveStat(msg.frequency, CoreUtils::msInSecond());
            }
        }

        switch (status)
        {
        case ADSL::Protocol::RxStatudeCode::OK:
            break;
        case ADSL::Protocol::RxStatudeCode::CRC_FAILED:
            statistics.fecErr += 1;
            break;
        case ADSL::Protocol::RxStatudeCode::UNSUPORTED_PROTOCOL_VERSION:
            statistics.protocolVersionErr += 1;
            break;
        case ADSL::Protocol::RxStatudeCode::UNSUPORTED_DECRYPTION_KEY:
            statistics.decryptionKeyErr += 1;
            break;
        case ADSL::Protocol::RxStatudeCode::UNSUPORTED_ERROR_CONTROL_FEC:
            statistics.unsupportedFec += 1;
            break;
        case ADSL::Protocol::RxStatudeCode::UNSUPPORTED_PAYLOAD:
            statistics.payloadUnSupportedErr += 1;
            break;
        default:
            statistics.unknownErr += 1;
            break;
        }
    }
}

bool ADSLAce::adsl_sendFrame(const void *ctx, const uint8_t *data, size_t lengthBytes)
{
    (void)ctx;
    (void)lengthBytes;

    const Tx_Struct *params = static_cast<const Tx_Struct *>(ctx);
    getBus().receive(GATAS::RadioTxFrameMsg{
        params->radioParameters,
        data,
        lengthBytes,
        params->radioNo});

    statistics.transmittedAircraftPositions += 1;
    return true;
}

void ADSLAce::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    ownshipPosition = SpinlockGuard::copyWithLock(CoreUtils::sharedSpinLock(), msg.position);
}

void ADSLAce::on_receive(const GATAS::GpsStatsMsg &msg)
{
    gpsStats = msg;
}

void ADSLAce::on_receive(const GATAS::RadioTxPositionRequestMsg &msg)
{
    // All RadioTX messages always come from one FreeRTOS Task, so guaranteed to be task safe
    if (msg.radioParameters.config->isTxDataSource(GATAS::DataSource::ADSLM))
    {
        auto ownship = SpinlockGuard::copyWithLock(CoreUtils::sharedSpinLock(), ownshipPosition);

        protocol.addPayloadLength = true;
        rqMBandRadioParameters = Tx_Struct{msg.radioParameters, msg.radioNo};
        protocol.tick(ownship.lat, ownship.lon, ownship.ellipseHeight, ownship.groundSpeed, ownship.track);
        protocol.rqSendTrafficPayload(&rqMBandRadioParameters);
    }
    else if (msg.radioParameters.config->isTxDataSource(GATAS::DataSource::ADSLO_HDR))
    {
        auto ownship = SpinlockGuard::copyWithLock(CoreUtils::sharedSpinLock(), ownshipPosition);

        rqOBandRadioParameters = Tx_Struct{msg.radioParameters, msg.radioNo};
        protocol.addPayloadLength = false;
        protocol.tick(ownship.lat, ownship.lon, ownship.ellipseHeight, ownship.groundSpeed, ownship.track);
        protocol.rqSendTrafficPayload(&rqOBandRadioParameters);
    }
}

// Called when a payload/header/status is received by the protocol layer
void ADSLAce::adsl_receivedTraffic(const ADSL::Header &header, const ADSL::TrafficPayload &tp)
{
    auto ownship = SpinlockGuard::copyWithLock(CoreUtils::sharedSpinLock(), ownshipPosition);
    statistics.receivedAircraftPositions += 1;

    // Own Address
    if (header.address().asUint() == ownship.conspicuity.icaoAddress)
    {
        return;
    }

    auto latitude = tp.latitude();
    auto longitude = tp.longitude();
    auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownship.lat, ownship.lon, tp.latitude(), tp.longitude());

    if (fromOwn.distance > distanceIgnore)
    {
        statistics.outOfDistance += 1;
        return;
    }
    // printf("ADSL: address:%06X latitude:%0.6f longitude:%0.6f altitude:%ld climbRate:%0.2f speed:%0.2f heading:%0.2f \n",
    // packet.address, fLatitude, fLongitude, packet.getaltitudeGeoid(), packet.getVerticalRate(), packet.getGroundSpeed(), packet.getTrack());
    auto epochMs = CoreUtils::msSinceEpoch();
    auto msElapsed = etl::max(static_cast<uint32_t>(0), static_cast<uint32_t>(epochMs - tp.timestampRestored(epochMs)));
    GATAS::AircraftPositionMsg aircraftPosition{
        GATAS::AircraftPositionInfo{
            CoreUtils::timeUs32() - (msElapsed * 1000),
            "",
            header.address().asUint(),
            addressMapToAddressType(header.addressMappingTable()),
            GATAS::DataSource::ADSLM,
            mapAircraftCategory(tp.aircraftCategory()),
            false,
            false,
            tp.flightState() == ADSL::TrafficPayload::FlightState::AIRBORNE ? true : false, // airBorn
            latitude,
            longitude,
            tp.altitude(),
            tp.verticalRate(),
            tp.speed(),
            static_cast<int16_t>(tp.groundTrack()),
            0.f,
            fromOwn.distance,
            fromOwn.relNorth,
            fromOwn.relEast},
        0};
    getBus().receive(aircraftPosition);
    (void)tp;
}

void ADSLAce::adsl_receivedStatus(const ADSL::Header &header, const ADSL::StatusPayload &sp)
{
    (void)sp;
    (void)header;
    GATAS_INFO("Received Status Payload");
}

void ADSLAce::adsl_buildTraffic(const void *ctx, ADSL::TrafficPayload &tp)
{
    (void)ctx;

    using ADSL::TrafficPayload;
    auto ownship = SpinlockGuard::copyWithLock(CoreUtils::sharedSpinLock(), ownshipPosition);
    protocol.setAddress(ownship.conspicuity.icaoAddress);

    tp.latitude(ownship.lat);
    tp.longitude(ownship.lon);
    tp.speed(ownship.groundSpeed);
    tp.altitude(ownship.ellipseHeight);
    tp.verticalRate(ownship.verticalSpeed);
    tp.groundTrack(ownship.track);
    tp.flightState(ownship.groundSpeed > GATAS::GROUNDSPEED_CONSIDERING_AIRBORN ? TrafficPayload::FlightState::AIRBORNE : ADSL::TrafficPayload::FlightState::ON_GROUND);
    tp.aircraftCategory(mapAircraftCategory(ownship.conspicuity.category));

    constexpr float uereDGPS = 2.7f;
    constexpr float uereGPS = 7.0f;
    auto uere = gpsStats.gpsFix.fixType == GATAS::GpsFixType::DGPS ? uereDGPS : uereGPS;
    float hfom = gpsStats.hDop > 0.0f ? gpsStats.hDop * uere : -1.0f;       // meters (horizontal)
    float vfom = gpsStats.vDop > 0.0f ? gpsStats.vDop * uere : -1.0f;       // meters (vertical)
    float hpl = gpsStats.pDop > 0.0f ? gpsStats.pDop * uere * 2.0f : -1.0f; // meters (integrity / protection level)

    tp.navigationIntegrity(mapHPL(hpl));
    tp.horizontalPositionAccuracy(mapHFOM(hfom));
    tp.verticalPositionAccuracy(mapVFOM(vfom));
}

void ADSLAce::adsl_buildStatusPayload(const void *ctx, ADSL::StatusPayload &sp)
{
    (void)ctx;
    GATAS_INFO("Send Status Payload");
    sp.mBandReceiveCapability(ADSL::StatusPayload::ReceiveCapability::Partial);
    sp.oBandHdrReceiveCapability(ADSL::StatusPayload::ReceiveCapability::Partial);

    // TODO: Find a way to correctly know what
    sp.eReceiveConspicuityBits(ADSL::StatusPayload::EConspicuityBits::EC_FANET);
    sp.eReceiveConspicuityBits(ADSL::StatusPayload::EConspicuityBits::EC_FLARM);
    sp.eReceiveConspicuityBits(ADSL::StatusPayload::EConspicuityBits::EC_OGN);
    sp.eTransmitConspicuityBits(ADSL::StatusPayload::EConspicuityBits::EC_FANET);
    sp.eTransmitConspicuityBits(ADSL::StatusPayload::EConspicuityBits::EC_FLARM);
    sp.eTransmitConspicuityBits(ADSL::StatusPayload::EConspicuityBits::EC_OGN);
}

uint8_t *ADSLAce::adsl_alloc(const void *ctx, size_t sizeBytes)
{
    (void)ctx;
    return static_cast<uint8_t *>(getGlobalPool().alloc(sizeBytes));
}

ADSL::TrafficPayload::AircraftCategory ADSLAce::mapAircraftCategory(GATAS::AircraftCategory category)
{
    using A = ADSL::TrafficPayload::AircraftCategory;
    using B = GATAS::AircraftCategory;

    // clang-format off
    switch (category)
    {
        case B::UNKNOWN:                  return A::NO_INFO;
        case B::LIGHT:                    return A::LIGHT_FIXED_WING;
        case B::SMALL:                    return A::SMALL_TO_HEAVY_FIXED_WING;
        case B::LARGE:                    return A::SMALL_TO_HEAVY_FIXED_WING;
        case B::HIGH_VORTEX:              return A::SMALL_TO_HEAVY_FIXED_WING;
        case B::HEAVY_ICAO:               return A::SMALL_TO_HEAVY_FIXED_WING;

        case B::AEROBATIC:                return A::LIGHT_FIXED_WING;
        case B::ROTORCRAFT:               return A::LIGHT_ROTORCRAFT;

        case B::GLIDER:                   return A::GLIDER_SAILPLANE;
        case B::LIGHT_THAN_AIR:           return A::LIGHTER_THAN_AIR;
        case B::SKY_DIVER:                return A::PARACHUTIST;
        case B::ULTRA_LIGHT_FIXED_WING:   return A::MODEL_PLANE;

        case B::UN_MANNED:                return A::UAS_OPEN_CATEGORY;
        case B::GYROCOPTER:               return A::GYROCOPTER;
        case B::HANG_GLIDER:              return A::HANGGLIDER;
        case B::PARA_GLIDER:              return A::PARAGLIDER;

        default:
            return A::NO_INFO;
    }
    // clang-format on
}

GATAS::AircraftCategory ADSLAce::mapAircraftCategory(ADSL::TrafficPayload::AircraftCategory category)
{
    using A = ADSL::TrafficPayload::AircraftCategory;
    using B = GATAS::AircraftCategory;

    // clang-format off
    switch (category)
    {
        case A::NO_INFO:                   return B::UNKNOWN;
        case A::LIGHT_FIXED_WING:          return B::LIGHT;
        case A::SMALL_TO_HEAVY_FIXED_WING: return B::SMALL;   // lossy
        case A::LIGHT_ROTORCRAFT:          return B::ROTORCRAFT;
        case A::HEAVY_ROTORCRAFT:          return B::ROTORCRAFT;
        case A::GLIDER_SAILPLANE:          return B::GLIDER;
        case A::LIGHTER_THAN_AIR:          return B::LIGHT_THAN_AIR;
        case A::ULTRALIGHT_HANGGLIDER:     return B::HANG_GLIDER;
        case A::HANGGLIDER:                return B::HANG_GLIDER;
        case A::PARAGLIDER:                return B::PARA_GLIDER;
        case A::PARACHUTIST:               return B::SKY_DIVER;
        case A::GYROCOPTER:                return B::GYROCOPTER;
        case A::MODEL_PLANE:               return B::ULTRA_LIGHT_FIXED_WING;

        case A::UAS_OPEN_CATEGORY:
        case A::UAS_SPECIFIC_CATEGORY:
        case A::UAS_CERTIFIED_CATEGORY:
            return B::UN_MANNED;

        case A::EVTOL_UAM:                return B::ROTORCRAFT; // closest lie
        case A::PARAMOTOR:                return B::PARA_GLIDER;

        default:
            return B::UNKNOWN;
    }
    // clang-format on
}

ADSL::TrafficPayload::HorizontalPositionAccuracy ADSLAce::mapHFOM(float hfomMeters)
{
    using H = ADSL::TrafficPayload::HorizontalPositionAccuracy;

    // clang-format off
  if (hfomMeters < 0.0f) { return H::UNKNOWN_NO_FIX; }
  if (hfomMeters < 3.0f)        { return H::HFOM_LESS_3M; }
  if (hfomMeters < 10.0f)       { return H::HFOM_3_10M; }
  if (hfomMeters < 30.0f)       { return H::HFOM_10_30M; }
  if (hfomMeters < 0.05f * 1852.0f) { return H::HFOM_30M_0_05NM; } // 0.05 NM ≈ 92.6 m
  if (hfomMeters < 0.1f  * 1852.0f) { return H::HFOM_0_05_0_1NM; }
  if (hfomMeters < 0.3f  * 1852.0f) { return H::HFOM_0_1_0_3NM; }
  if (hfomMeters < 0.5f  * 1852.0f) { return H::HFOM_0_3_0_5NM; }
    // clang-format on
    return H::UNKNOWN_NO_FIX;
}

ADSL::TrafficPayload::VerticalPositionAccuracy ADSLAce::mapVFOM(float vfomMeters)
{
    using V = ADSL::TrafficPayload::VerticalPositionAccuracy;
    // clang-format off
  if (vfomMeters < 0.0f) { return V::UNKNOWN_NO_FIX; }
  if (vfomMeters < 10.0f)  { return V::VFOM_LESS_10M; }
  if (vfomMeters < 45.0f)  { return V::VFOM_10_45M; }
  if (vfomMeters < 150.0f) { return V::VFOM_45_150M; }
    // clang-format on
    return V::UNKNOWN_NO_FIX;
}

ADSL::TrafficPayload::NavigationIntegrity ADSLAce::mapHPL(float hplMeters)
{
    using N = ADSL::TrafficPayload::NavigationIntegrity;
    // clang-format off
  if (hplMeters < 0.0f) { return N::UNDEFINED; }
  if (hplMeters < 7.5f)    { return N::RC_LESS_7_5M; }
  if (hplMeters < 25.0f)   { return N::RC_7_5_25M; }
  if (hplMeters < 75.0f)   { return N::RC_25_75M; }
  if (hplMeters < 185.2f)  { return N::RC_75M_0_1NM; }
  if (hplMeters < 370.4f)  { return N::RC_0_1_0_2NM; }
  if (hplMeters < 1111.2f) { return N::RC_0_2_0_6NM; }
  if (hplMeters < 1852.0f) { return N::RC_0_6_1NM; }
  if (hplMeters < 3704.0f) { return N::RC_1_2NM; }
  if (hplMeters < 7408.0f) { return N::RC_2_4NM; }
  if (hplMeters < 14816.0f){ return N::RC_4_8NM; }
  if (hplMeters < 37040.0f){ return N::RC_8_20NM; }
    // clang-format on
    return N::RC_20NM_OR_MORE;
}

ADSL::TrafficPayload::VelocityAccuracy ADSLAce::mapVelAcc(float accVelMps)
{
    using V = ADSL::TrafficPayload::VelocityAccuracy;
    // clang-format off
  if (accVelMps <= 0.0f) { return V::UNKNOWN_NO_FIX; }
  if (accVelMps < 1.0f)  { return V::ACC_VEL_LESS_1MS; }
  if (accVelMps < 3.0f)  { return V::ACC_VEL_1_3MS; }
  if (accVelMps < 10.0f) { return V::ACC_VEL_3_10MS; }
    // clang-format on
    return V::UNKNOWN_NO_FIX;
}

GATAS::AddressType ADSLAce::addressMapToAddressType(uint8_t addressMap) const
{
    switch (addressMap)
    {
    case 0x00:
        return GATAS::AddressType::RANDOM;
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
        return GATAS::AddressType::RESERVED;
    case 0x05:
        return GATAS::AddressType::ICAO;
    case 0x06:
        return GATAS::AddressType::FLARM;
    case 0x07:
        return GATAS::AddressType::OGN;
    case 0x08:
        return GATAS::AddressType::FANET;
    // Not a bug, if we don't the the type we just say random
    default:
        return GATAS::AddressType::UNKNOWN;
    }
}

uint8_t ADSLAce::addressTypeToAddressMap(GATAS::AddressType addressType)
{
    switch (addressType)
    {
    case GATAS::AddressType::RANDOM:
        return 0x00;
    case GATAS::AddressType::RESERVED:
        return 0x01;
    case GATAS::AddressType::ICAO:
        return 0x05;
    case GATAS::AddressType::FLARM:
        return 0x06;
    case GATAS::AddressType::OGN:
        return 0x07;
    case GATAS::AddressType::FANET:
        return 0x08;
    default:
        return 0x00;
    }
}