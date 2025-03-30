#include <stdio.h>

#include "dataport.hpp"
#include "ace/semaphoreguard.hpp"
#include "ace/moreutils.hpp"

void DataPort::on_receive(const OpenAce::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName != DataPort::NAME)
    {
        return;
    }

    address = openAceConfiguration.address;
    category = openAceConfiguration.category;
}

void DataPort::on_receive(const OpenAce::OwnshipPositionMsg &msg)
{
    static Every<uint32_t, 500'000, 1'000'000> sendValidGps{0};
    ownshipPosition = msg.position;
    if (sendValidGps.isItTime(CoreUtils::timeUs32())) {

        // sendGPRMC(message.position);
        // sendGPGSA(ownshipPosition);
        // sendGPGGA(ownshipPosition);

        sendPGRMZ(msg.position);
        sendPFLAU(msg.position);
    }
}

void DataPort::on_receive(const OpenAce::TrackedAircraftPositionMsg &msg)
{
    sendPFLAA(msg.position);
}

void DataPort::on_receive(const OpenAce::GPSSentenceMsg &msg)
{
    OpenAce::NMEAString sentence = msg.sentence;
    sentence.append("\r\n");
    getBus().receive(OpenAce::DataPortMsg{sentence});
    statistics.messages++;
}

/**
 * PFLAA – Data on other proximate aircraft
 * Periodicity:
 * Sent when available. Can be sent several times per second with information on several (but not always all) surrounding targets.
 */
void DataPort::sendPFLAA(const OpenAce::AircraftPositionInfo &position)
{
    OpenAce::NMEAString pflaa;
    etl::string_stream stream(pflaa);

    etl::string<6> groundSpeed;
    getPFLAAGroundSpeed(position, groundSpeed);

    etl::string<7> climbRate;
    getPFLAAClimbRate(position, climbRate);

    // Example: PFLAA: $PFLAA,0,10,7,21,0,B1B1B1,0,,,-0.1,1,48,0,*23
    stream << "$PFLAA,"
              "0,"                                                            // @todo Alarm Level
           << position.relNorthFromOwn << ","                                 // Relative North Meters
           << position.relEastFromOwn << ","                                  // Relative East Meters
           << (position.altitudeWgs84 - ownshipPosition.altitudeWgs84) << "," // Relative Vertical Meters
           << getPFLAAAddressType(position.addressType) << ","                // ID Type
           << position.icaoAddress << ","                                     // HEXCode example 484FB3!PH-DHA
           << position.course << ","                                          // Heading
           << ","                                                             // TurnRate kept empty
           << groundSpeed << ","                                              // Ground Speed
           << climbRate << ","                                                // Climb Rate
           << getPFLAAAircraftCategory(position) << ","                       // Aircraft Type
           << position.noTrack << ","                                         // Tracking
           << getPFLAASourceType(position) << ","                             // Source Type
           << "";                                                             // RSSI

    CoreUtils::addChecksumToNMEA(pflaa);
    // printf("pflaa t:%08ld %s\n", CoreUtils::timeUs32() / 1'000, pflaa.c_str());
    getBus().receive(OpenAce::DataPortMsg{pflaa});
    statistics.messages++;
}

uint8_t DataPort::getPFLAASourceType(const OpenAce::AircraftPositionInfo &position)
{
    switch (position.dataSource)
    {
    case OpenAce::DataSource::ADSB:
        return 1;
    case OpenAce::DataSource::MODES:
        return 6;
    default:
        // FLARM OGN FANET
        return 0;
    }
}

uint8_t DataPort::getPFLAAAddressType(const OpenAce::AddressType type)
{
    //        return type==OpenAce::AddressType::OGN?1:static_cast<uint8_t>(type);
    uint8_t itype = static_cast<uint8_t>(type);
    return itype > 2 ? 1 : itype;
}

void DataPort::getPFLAAGroundSpeed(const OpenAce::AircraftPositionInfo &position, etl::string<6> &speed)
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

void DataPort::getPFLAAClimbRate(const OpenAce::AircraftPositionInfo &position, etl::string<7> &verticalSpeed)
{
    etl::format_spec clibRateFormat = etl::format_spec().precision(1);

    // if (position.stealth /* || ownship.privacy */)
    // {
    //     return;
    // }

    etl::to_string(position.verticalSpeed, verticalSpeed, clibRateFormat);
}

etl::string_view DataPort::getPFLAAAircraftCategory(const OpenAce::AircraftPositionInfo &position) const
{
    static constexpr etl::string_view hexLookup[] = {
        "0", "1", "2", "3", "4", "5", "6", "7",
        "8", "9", "A", "B", "C", "D", "E", "F"
    };
    auto aircraftTypeHex = static_cast<uint8_t>(position.aircraftType);

    if (aircraftTypeHex > 0x0F)
    {
        return  hexLookup[0x0A];
    }

    return hexLookup[aircraftTypeHex];
}

/**
 * PFLAU – Heartbeat, status, and basic alarms
 *
 * Periodicity:
 * Sent once every second (1.8 s at maximum)
 */

void DataPort::sendPFLAU(const OpenAce::OwnshipPositionInfo &position)
{    
    (void)position;
        OpenAce::NMEAString pflau;
        etl::string_stream stream(pflau);

        //  uint8_t neighbours=0;
        uint8_t gpsState = 2;
        stream << "$PFLAU,"
                "1," // RX does skydemon need 1 here?
                "1," // TX
            << gpsState << ","
                            "1,"                                              // Power   // Over or under voltage
                            "0,"                                              // Alarm Level
                            "0,"                                              // Relative Bearing
                            "2,"                                              // Alarm Type 0=No aircraft ithin range, 2=Aircraft alarm, 3=Obstacle/Alert Zone Alarm
                            "100,"                                            // Relative Vertical
                            "1000,"                                           // Relative Distance
            << OpenAce::ICAO_HEX_FORMAT << address << OpenAce::RESET_FORMAT; // ID

        // example $PFLAU,1,1,2,1,0,,0,,,*4D
        CoreUtils::addChecksumToNMEA(pflau);
        getBus().receive(OpenAce::DataPortMsg{pflau});
        statistics.messages++;
}

/**
 * PFLAE – Self-test result and errors codes
 *
 * Periodicity:
 * Sent once after startup and completion of self-test, when errors occur, and when requested
 *
 */
void DataPort::sendPFLAE(const OpenAce::AircraftPositionInfo &position)
{
    (void)position;
    // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
    OpenAce::NMEAString nmeaString = "$PFLAE,A,0,0";

    CoreUtils::addChecksumToNMEA(nmeaString);
}

/**
 * PFLAV – Version information
 *
 * Periodicity:
 * Sent once after startup and completion of self-test and when requested
 *
 */
void DataPort::sendPFLAV(const OpenAce::AircraftPositionInfo &position)
{
    (void)position;
    // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
    OpenAce::NMEAString nmeaString = "$PFLAV,A,0,0";
}

/**
 * PFLAR – Reset
 *
 * Periodicity:
 * Not applicable
 *
 */
void DataPort::sendPFLAR(const OpenAce::AircraftPositionInfo &position)
{
    (void)position;
    // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
    //        OpenAce::NMEAString nmeaString="$PFLAR,A,0,0";
}

/**
 * PFLAC – Device configuration
 *
 * Periodicity:
 * Sent when requested
 */

void DataPort::sendPFLAC(const OpenAce::AircraftPositionInfo &position)
{
    (void)position;
    // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
    OpenAce::NMEAString nmeaString = "$PFLAC,HELLO,GLIDER_PILOTS";
}

/**
 * PFLAN – Continuous Analyzer of Radio Performance (CARP).
 *
 * Periodicity:
 * Sent when requested
 */
void DataPort::sendPFLANC(const OpenAce::AircraftPositionInfo &position)
{
    (void)position;
    // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
    // OpenAce::NMEAString nmeaString="$PFLANC,A,ERROR";
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
void DataPort::sendGPRMC(const OpenAce::AircraftPositionInfo &position)
{
    time_t timet;
    struct tm *ptm;
    time(&timet);
    ptm = gmtime(&timet);
    OpenAce::NMEAString gnrmc;
    etl::string_stream stream(gnrmc);

    int8_t latDeg, lonDeg;
    float latMin, lonMin;
    decimalDegreesToDMM(position.lat, latDeg, latMin);
    decimalDegreesToDMM(position.lon, lonDeg, lonMin);

    // SkyDemon want's to see GPRMC, not GNRMC
    stream << "$GPRMC,"
           << etl::format_spec{}.width(2).fill('0') << ptm->tm_hour << ptm->tm_min << ptm->tm_sec << OpenAce::RESET_FORMAT << "." << position.timestamp % 1000 << ","                              // @todo validate if position.timestamp%1000 is correct
           << "A,"                                                                                                                                                                                 // validity - A-ok, V-invalid
           << etl::format_spec{}.width(2).fill('0') << latDeg << etl::format_spec{}.precision(5).width(8).fill('0') << latMin << OpenAce::RESET_FORMAT << "," << (position.lat >= 0 ? "N," : "S,") // Latitude
           << etl::format_spec{}.width(3).fill('0') << lonDeg << etl::format_spec{}.precision(5).width(8).fill('0') << lonMin << OpenAce::RESET_FORMAT << "," << (position.lon >= 0 ? "E," : "W,") // Longitude
           << position.groundSpeed * MS_TO_KN << ","                                                                                                                                               // Speed over ground
           << position.course << ","                                                                                                                                                               // Course over ground
           << etl::format_spec{}.width(2).fill('0') << ptm->tm_mday << ptm->tm_mon + 1 << (ptm->tm_year + 1900 - 2000) << OpenAce::RESET_FORMAT << ","                                             // Date
           << ","                                                                                                                                                                                  // Magnetic variation
           << ","                                                                                                                                                                                  // Magnetic variation direction
           << "D";                                                                                                                                                                                 // A = Autonomous, D = DGPS, E =DR

    CoreUtils::addChecksumToNMEA(gnrmc);
    getBus().receive(OpenAce::DataPortMsg{gnrmc});
    statistics.messages++;
}

void DataPort::sendPGRMZ(const OpenAce::OwnshipPositionInfo &position)
{
    //         // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
    OpenAce::NMEAString pgrmz;
    etl::string_stream stream(pgrmz);

    stream << "$PGRMZ,"
           << position.altitudeWgs84 << "," // @todo Convert by estimate or read out barometric altitude
           << "f,"                          // Altitude unit
           << "3";                          // @todo Fix type 2=2D 3=3D

    // example $PGRMZ,295,f,3*15
    CoreUtils::addChecksumToNMEA(pgrmz);
    getBus().receive(OpenAce::DataPortMsg{pgrmz});
    statistics.messages++;
}

/**
 * GNGSA – GPS DOP and active satellites
 */
void DataPort::sendGNGSA(const OpenAce::AircraftPositionInfo &position)
{
    (void)position;
    // (int32_t)round(relNorth),(int32_t)round(relEast),(int32_t)round(relVert),movePilotData->addressType, movePilotData->DevId.c_str(),(int32_t)round(movePilotData->heading),currentSpeed,movePilotData->climb,uint8_t(movePilotData->aircraftType));
    // OpenAce::NMEAString nmeaString="$PFLANC,A,ERROR";

    OpenAce::NMEAString gngsa{"$GNGSA,A,3,,,,,,,,,,,,,1.0,1.0,1.0"};
    CoreUtils::addChecksumToNMEA(gngsa);
    getBus().receive(OpenAce::DataPortMsg{gngsa});
    statistics.messages++;
}

void DataPort::sendGPGGA(const OpenAce::AircraftPositionInfo &position)
{
    (void)position;
    time_t timet;
    struct tm *ptm;
    time(&timet);
    ptm = gmtime(&timet);

    OpenAce::NMEAString gngga;
    etl::string_stream stream(gngga);

    int8_t latDeg, lonDeg;
    float latMin, lonMin;
    decimalDegreesToDMM(position.lat, latDeg, latMin);
    decimalDegreesToDMM(position.lon, lonDeg, lonMin);

    // SkyDemon want's to see GPGGA not GNGGA
    stream << "$GPGGA,"
           << etl::format_spec{}.width(2).fill('0') << ptm->tm_hour << ptm->tm_min << ptm->tm_sec << OpenAce::RESET_FORMAT << "." << position.timestamp % 1000 << ","                              // @todo validate if position.timestamp%1000 is correct
           << etl::format_spec{}.width(2).fill('0') << latDeg << etl::format_spec{}.precision(5).width(8).fill('0') << latMin << OpenAce::RESET_FORMAT << "," << (position.lat >= 0 ? "N," : "S,") // Latitude
           << etl::format_spec{}.width(3).fill('0') << lonDeg << etl::format_spec{}.precision(5).width(8).fill('0') << lonMin << OpenAce::RESET_FORMAT << "," << (position.lon >= 0 ? "E," : "W,") // Longitude
           << "2,"                                                                                                                                                                                 // 1==GPS, 2=DGPS, 9=DBAS
           << "6,"                                                                                                                                                                                 // Number of satellites
           << "1.0,"                                                                                                                                                                               // Horizontal dilution of position (hdop)
           << position.altitudeWgs84 << ","                                                                                                                                                        // Altitude Wgs84
           << "M,"                                                                                                                                                                                 // Altitude unit
           << "0,"                                                                                                                                                                                 // Geoid Separation
           << "M,"                                                                                                                                                                                 // Unit
           << ","                                                                                                                                                                                  // age of differential GPS data
           << "";                                                                                                                                                                                  // Differential reference station ID

    CoreUtils::addChecksumToNMEA(gngga);
    getBus().receive(OpenAce::DataPortMsg{gngga});
    statistics.messages++;
}

/**
 * We seem to need this to get altitude in SkyDemon
 */
void DataPort::sendGPGSA()
{
    OpenAce::NMEAString gpgsa;
    etl::string_stream stream(gpgsa);

    stream << "$GPGSA,A,3,,,,,,,,,,,,,1.0,1.0,1.0";
    CoreUtils::addChecksumToNMEA(gpgsa);
    getBus().receive(OpenAce::DataPortMsg{gpgsa});
    statistics.messages++;
}


void DataPort::sendLK8EX1() {
    OpenAce::NMEAString gpgsa;
    etl::string_stream stream(gpgsa);
    stream << "$LK8EX1,999999,999999,9999,999,999";
    CoreUtils::addChecksumToNMEA(gpgsa);
    getBus().receive(OpenAce::DataPortMsg{gpgsa});
    statistics.messages++;
}

void DataPort::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"messages\":" << statistics.messages;
    stream << "}\n";
}