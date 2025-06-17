#include <stdio.h>

#include "dataport.hpp"
#include "ace/semaphoreguard.hpp"
#include "ace/moreutils.hpp"

void DataPort::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::CONFIG)
    {
        auto newConfig = msg.config.gaTasConfig();
        address = newConfig.address;
        category = newConfig.category;
    }
}

void DataPort::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    static Every<uint32_t, 500'000, 1'000'000> sendValidGps{0};
    ownshipPosition = msg.position;
    if (sendValidGps.isItTime(CoreUtils::timeUs32Raw()))
    {

        // Initially the idea was to 'emulate' a GNS chip, but later decides
        // to pass through GPS messages. Keeping this code just in case/
        // sendGPRMC(message.position);
        // sendGPGSA(ownshipPosition);
        // sendGPGGA(ownshipPosition);
        // sendPGRMZ(msg.position);
        sendPFLAU(msg.position);
    }
}

void DataPort::on_receive(const GATAS::TrackedAircraftPositionMsg &msg)
{
    sendPFLAA(msg.position);
}

void DataPort::on_receive(const GATAS::GPSSentenceMsg &msg)
{
    GATAS::NMEAString sentence = msg.sentence;
    sentence.append("\r\n");
    getBus().receive(GATAS::DataPortMsg{sentence});
    statistics.messages++;
}

/**
 * PFLAA – Data on other proximate aircraft
 * Periodicity:
 * Sent when available. Can be sent several times per second with information on several (but not always all) surrounding targets.
 */
void DataPort::sendPFLAA(const GATAS::AircraftPositionInfo &position)
{
    GATAS::NMEAString pflaa;
    etl::string_stream stream(pflaa);

    etl::string<6> groundSpeed;
    getPFLAAGroundSpeed(position, groundSpeed);

    etl::string<7> climbRate;
    getPFLAAClimbRate(position, climbRate);

    // Example: PFLAA: $PFLAA,0,10,7,21,0,B1B1B1,0,,,-0.1,1,48,0,*23
    stream << "$PFLAA,"
              "0,"                                                        // @todo Alarm Level
           << position.relNorthFromOwn << ","                             // Relative North Meters
           << position.relEastFromOwn << ","                              // Relative East Meters
           << (position.altitudeHAE - ownshipPosition.altitudeHAE) << "," // Relative Vertical Meters
           << getPFLAAAddressType(position.addressType) << ",";           // ID Type
    CoreUtils::streamIcaoAddress(stream, position.address, position.addressType, position.callSign);
    stream << ","                                       // HEXCode example 484FB3!PH-DHA
           << position.course << ","                    // Heading
           << ","                                       // TurnRate kept empty
           << groundSpeed << ","                        // Ground Speed
           << climbRate << ","                          // Climb Rate
           << getPFLAAAircraftCategory(position) << "," // Aircraft Type
           << position.noTrack << ","                   // Tracking
           << getPFLAASourceType(position) << ","       // Source Type
           << "";                                       // RSSI

    CoreUtils::addChecksumToNMEA(pflaa);
    // printf("t:%08ld %s", CoreUtils::timeMs32(), pflaa.c_str());
    getBus().receive(GATAS::DataPortMsg{pflaa});
    statistics.messages++;
}

uint8_t DataPort::getPFLAASourceType(const GATAS::AircraftPositionInfo &position)
{
    switch (position.dataSource)
    {
    case GATAS::DataSource::ADSB:
        return 1;
    case GATAS::DataSource::MODES:
        return 6;
    default:
        // FLARM OGN FANET
        return 0;
    }
}

uint8_t DataPort::getPFLAAAddressType(const GATAS::AddressType type)
{
    //        return type==GATAS::AddressType::OGN?1:static_cast<uint8_t>(type);
    uint8_t itype = static_cast<uint8_t>(type);
    return itype > 2 ? 1 : itype;
}

// Groundspeed is in m/s for PFLAA
void DataPort::getPFLAAGroundSpeed(const GATAS::AircraftPositionInfo &position, etl::string<6> &speed)
{
    // if (position.stealth /* || ownship.notrack */)
    // {
    //     return;
    // }
    // if (!position.airborne)
    // {
    //     return;
    // }

    etl::to_string(static_cast<uint16_t>(position.groundSpeed + 0.5f), speed);
}

void DataPort::getPFLAAClimbRate(const GATAS::AircraftPositionInfo &position, etl::string<7> &verticalSpeed)
{
    etl::format_spec clibRateFormat = etl::format_spec().precision(1);

    // if (position.stealth /* || ownship.privacy */)
    // {
    //     return;
    // }

    etl::to_string(position.verticalSpeed, verticalSpeed, clibRateFormat);
}

etl::string_view DataPort::getPFLAAAircraftCategory(const GATAS::AircraftPositionInfo &position) const
{
    static constexpr etl::string_view hexLookup[] = {
        "0", "1", "2", "3", "4", "5", "6", "7",
        "8", "9", "A", "B", "C", "D", "E", "F"};
    auto aircraftTypeHex = static_cast<uint8_t>(position.aircraftType);

    if (aircraftTypeHex > 0x0F)
    {
        return hexLookup[0x0A];
    }

    return hexLookup[aircraftTypeHex];
}

/**
 * PFLAU – Heartbeat, status, and basic alarms
 *
 * Periodicity:
 * Sent once every second (1.8 s at maximum)
 */

void DataPort::sendPFLAU(const GATAS::OwnshipPositionInfo &position)
{
    (void)position;
    GATAS::NMEAString pflau;
    etl::string_stream stream(pflau);

    // $PFLAU,5,1,1,1,0,,0,,,*
    stream << "$PFLAU,"
              "0"  // Should be filled in with the number of aircraft we are tracking But does not seem to be required
              ",1" // Idially should based on tranceiver status
              ",1,1,0,,0,,,";

    CoreUtils::addChecksumToNMEA(pflau);
    getBus().receive(GATAS::DataPortMsg{pflau});
    statistics.messages++;
}

/**
 * PFLAE – Self-test result and errors codes
 *
 * Periodicity:
 * Sent once after startup and completion of self-test, when errors occur, and when requested
 *
 */
void DataPort::sendPFLAE(const GATAS::AircraftPositionInfo &position)
{
    (void)position;
    // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
    GATAS::NMEAString nmeaString = "$PFLAE,A,0,0";

    CoreUtils::addChecksumToNMEA(nmeaString);
}

/**
 * PFLAV – Version information
 *
 * Periodicity:
 * Sent once after startup and completion of self-test and when requested
 *
 */
void DataPort::sendPFLAV(const GATAS::AircraftPositionInfo &position)
{
    (void)position;
    // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
    GATAS::NMEAString nmeaString = "$PFLAV,A,0,0";
}

/**
 * PFLAR – Reset
 *
 * Periodicity:
 * Not applicable
 *
 */
void DataPort::sendPFLAR(const GATAS::AircraftPositionInfo &position)
{
    (void)position;
    // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
    //        GATAS::NMEAString nmeaString="$PFLAR,A,0,0";
}

/**
 * PFLAC – Device configuration
 *
 * Periodicity:
 * Sent when requested
 */

void DataPort::sendPFLAC(const GATAS::AircraftPositionInfo &position)
{
    (void)position;
    // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
    GATAS::NMEAString nmeaString = "$PFLAC,HELLO,GLIDER_PILOTS";
}

/**
 * PFLAN – Continuous Analyzer of Radio Performance (CARP).
 *
 * Periodicity:
 * Sent when requested
 */
void DataPort::sendPFLANC(const GATAS::AircraftPositionInfo &position)
{
    (void)position;
    // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
    // GATAS::NMEAString nmeaString="$PFLANC,A,ERROR";
}

void DataPort::decimalDegreesToDMM(float degrees, int8_t &deg, float &min)
{
    // Convert decimal degrees to degrees and minutes
    deg = static_cast<int8_t>(degrees);
    min = (degrees - deg) * 60.0f;
}

/**
 * GPRMC – NMEA minimum recommended GPS navigation data
 */
// void DataPort::sendGPRMC(const GATAS::OwnshipPositionInfo &position)
// {
//     time_t timet;
//     struct tm *ptm;
//     time(&timet);
//     ptm = gmtime(&timet);
//     GATAS::NMEAString gnrmc;
//     etl::string_stream stream(gnrmc);

//     int8_t latDeg, lonDeg;
//     float latMin, lonMin;
//     decimalDegreesToDMM(position.lat, latDeg, latMin);
//     decimalDegreesToDMM(position.lon, lonDeg, lonMin);

//     // SkyDemon want's to see GPRMC, not GNRMC
//     stream << "$GPRMC,"
//            << etl::format_spec{}.width(2).fill('0') << ptm->tm_hour << ptm->tm_min << ptm->tm_sec << GATAS::RESET_FORMAT << "." << position.timestamp % 1000 << ","                              // @todo validate if position.timestamp%1000 is correct
//            << "A,"                                                                                                                                                                                 // validity - A-ok, V-invalid
//            << etl::format_spec{}.width(2).fill('0') << latDeg << etl::format_spec{}.precision(5).width(8).fill('0') << latMin << GATAS::RESET_FORMAT << "," << (position.lat >= 0 ? "N," : "S,") // Latitude
//            << etl::format_spec{}.width(3).fill('0') << lonDeg << etl::format_spec{}.precision(5).width(8).fill('0') << lonMin << GATAS::RESET_FORMAT << "," << (position.lon >= 0 ? "E," : "W,") // Longitude
//            << position.groundSpeed * MS_TO_KN << ","                                                                                                                                               // Speed over ground
//            << position.course << ","                                                                                                                                                               // Course over ground
//            << etl::format_spec{}.width(2).fill('0') << ptm->tm_mday << ptm->tm_mon + 1 << (ptm->tm_year + 1900 - 2000) << GATAS::RESET_FORMAT << ","                                             // Date
//            << ","                                                                                                                                                                                  // Magnetic variation
//            << ","                                                                                                                                                                                  // Magnetic variation direction
//            << "D";                                                                                                                                                                                 // A = Autonomous, D = DGPS, E =DR

//     CoreUtils::addChecksumToNMEA(gnrmc);
//     getBus().receive(GATAS::DataPortMsg{gnrmc});
//     statistics.messages++;
// }

// void DataPort::sendPGRMZ(const GATAS::OwnshipPositionInfo &position)
// {
//     //         // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
//     GATAS::NMEAString pgrmz;
//     etl::string_stream stream(pgrmz);

//     stream << "$PGRMZ,"
//            << (position.altitudeHAE - position.geoidSeparation) * M_TO_FT << "," // @todo: Not clear what altitude this really is
//            << "f,"                          // Altitude unit
//            << "3";                          // @todo Fix type 2=2D 3=3D

//     // example $PGRMZ,295,f,3*15
//     CoreUtils::addChecksumToNMEA(pgrmz);
//     getBus().receive(GATAS::DataPortMsg{pgrmz});
//     statistics.messages++;
// }

/**
 * GNGSA – GPS DOP and active satellites
 */
// void DataPort::sendGNGSA(const GATAS::AircraftPositionInfo &position)
// {
//     (void)position;
//     // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
//     // GATAS::NMEAString nmeaString="$PFLANC,A,ERROR";

//     GATAS::NMEAString gngsa{"$GNGSA,A,3,,,,,,,,,,,,,1.0,1.0,1.0"};
//     CoreUtils::addChecksumToNMEA(gngsa);
//     getBus().receive(GATAS::DataPortMsg{gngsa});
//     statistics.messages++;
// }

// void DataPort::sendGPGGA(const GATAS::OwnshipPositionInfo &position)
// {
//     (void)position;
//     time_t timet;
//     struct tm *ptm;
//     time(&timet);
//     ptm = gmtime(&timet);

//     GATAS::NMEAString gngga;
//     etl::string_stream stream(gngga);

//     int8_t latDeg, lonDeg;
//     float latMin, lonMin;
//     decimalDegreesToDMM(position.lat, latDeg, latMin);
//     decimalDegreesToDMM(position.lon, lonDeg, lonMin);

//     // SkyDemon want's to see GPGGA not GNGGA
//     stream << "$GPGGA,"
//            << etl::format_spec{}.width(2).fill('0') << ptm->tm_hour << ptm->tm_min << ptm->tm_sec << GATAS::RESET_FORMAT << "." << position.timestamp % 1000 << ","                              // @todo validate if position.timestamp%1000 is correct
//            << etl::format_spec{}.width(2).fill('0') << latDeg << etl::format_spec{}.precision(5).width(8).fill('0') << latMin << GATAS::RESET_FORMAT << "," << (position.lat >= 0 ? "N," : "S,") // Latitude
//            << etl::format_spec{}.width(3).fill('0') << lonDeg << etl::format_spec{}.precision(5).width(8).fill('0') << lonMin << GATAS::RESET_FORMAT << "," << (position.lon >= 0 ? "E," : "W,") // Longitude
//            << "2,"                                                                                                                                                                                 // 1==GPS, 2=DGPS, 9=DBAS
//            << "6,"                                                                                                                                                                                 // Number of satellites
//            << "1.0,"                                                                                                                                                                               // Horizontal dilution of position (hdop)
//            << (position.altitudeHAE - position.geoidSeparation) << ","                                                                                                                                                        // Altitude Wgs84
//            << "M,"                                                                                                                                                                                 // Altitude unit
//            << "position.geoidSeparation,"                                                                                                                                                                                 // Geoid Separation
//            << "M,"                                                                                                                                                                                 // Unit
//            << ","                                                                                                                                                                                  // age of differential GPS data
//            << "";                                                                                                                                                                                  // Differential reference station ID

//     CoreUtils::addChecksumToNMEA(gngga);
//     getBus().receive(GATAS::DataPortMsg{gngga});
//     statistics.messages++;
// }

/**
 * We seem to need this to get altitude in SkyDemon
 */
// void DataPort::sendGPGSA()
// {
//     GATAS::NMEAString gpgsa;
//     etl::string_stream stream(gpgsa);

//     stream << "$GPGSA,A,3,,,,,,,,,,,,,1.0,1.0,1.0";
//     CoreUtils::addChecksumToNMEA(gpgsa);
//     getBus().receive(GATAS::DataPortMsg{gpgsa});
//     statistics.messages++;
// }

void DataPort::sendLK8EX1()
{
    GATAS::NMEAString gpgsa;
    etl::string_stream stream(gpgsa);
    stream << "$LK8EX1,999999,999999,9999,99,999";
    CoreUtils::addChecksumToNMEA(gpgsa);
    getBus().receive(GATAS::DataPortMsg{gpgsa});
    statistics.messages++;
}

void DataPort::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"messages\":" << statistics.messages;
    stream << "}\n";
}