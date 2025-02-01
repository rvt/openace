#include <stdio.h>

#include "gpsdecoder.hpp"
#include "ace/moreutils.hpp"

extern const char *OpenAce_buildTime;

OpenAce::PostConstruct GpsDecoder::postConstruct()
{
    return OpenAce::PostConstruct::OK;
}

void GpsDecoder::start()
{
    getBus().subscribe(*this);
}

void GpsDecoder::stop()
{
    getBus().unsubscribe(*this);
}

void GpsDecoder::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    constexpr etl::format_spec width2fill0 = etl::format_spec().width(2).fill('0');
    const char *dopValue = OpenAce::DOPInterpretationToString(OpenAce::floatToDOPInterpretation(pDop));
    stream << "{";
    stream << "\"OpenAce_buildTime\": \"" << OpenAce_buildTime << "\"";
    stream << ",\"receivedGGA\":" << statistics.receivedGGA;
    stream << ",\"receivedRMC\":" << statistics.receivedRMC;
    stream << ",\"receivedGSA\":" << statistics.receivedGSA;
    stream << ",\"receivedOther\":" << statistics.receivedOther;
    stream << ",\"latitude\":" << etl::format_spec{}.precision(5) << latitude;
    stream << ",\"longitude\":" << longitude << etl::format_spec{}.precision(1);
    stream << ",\"altitudeWgs84\":" << altitudeWgs84();
    stream << ",\"heightGeoidWGS84\":" << heightGeoidWGS84;
    stream << ",\"groundspeed\":" << groundSpeed;
    stream << ",\"track\":" << course();
    stream << ",\"pDop\":" << pDop << OpenAce::RESET_FORMAT;
    stream << ",\"dopValue\":\"" << dopValue << "\"";
    stream << ",\"fixQuality\":\"" << fixQuality << "\"";
    stream << ",\"satellitesTracked\":" << satellitesTracked;
    stream << ",\"upTime\":" << (CoreUtils::timeS32() - statistics.startTime),
        stream << ",\"UtcTimeMsg\":" << "\""
               << width2fill0 << lastGGATimestamp.hours << OpenAce::RESET_FORMAT << ":"
               << width2fill0 << lastGGATimestamp.minutes << OpenAce::RESET_FORMAT << ":"
               << width2fill0 << lastGGATimestamp.seconds << OpenAce::RESET_FORMAT << "\"";
    stream << "}\n";
}

void GpsDecoder::on_receive(const OpenAce::GPSSentenceMsg &msg)
{
    // printf("GpsDecoder: %s\n", msg.sentence.c_str());
    static Every<uint32_t, 0, 15'000'000> sendUtcTimeMsg{0};       // every 15 seconds
    static Every<uint32_t, 10'000'000, 30'000'000> gpsStatsMsg{0}; // Every 30 seconds 10 seconds in

    switch (minmea_sentence_id(msg.sentence.c_str(), false))
    {
    case MINMEA_SENTENCE_RMC:
    {
        statistics.receivedRMC++;
        struct minmea_sentence_rmc frame;
        if (minmea_parse_rmc(&frame, msg.sentence.c_str()))
        {
            uint16_t millis = frame.time.microseconds / 1000;
            // TODO: Be more intelligent on when sending time messages so a 'fix' is quicker known after (re)start of the whole system
            if (millis == 0 && sendUtcTimeMsg.isItTime(CoreUtils::timeUs32()))
            {
                // The time in a RMC sentence is the UTC time, not GPS time
                getBus().receive(
                    OpenAce::UtcTimeMsg{
                        static_cast<int16_t>(frame.date.year + 2000),
                        static_cast<int8_t>(frame.date.month),
                        static_cast<int8_t>(frame.date.day),
                        static_cast<int8_t>(frame.time.hours),
                        static_cast<int8_t>(frame.time.minutes),
                        static_cast<int8_t>(frame.time.seconds),
                        static_cast<int16_t>(millis)});
            }

            // Update planes position when fix is valid
            if (frame.valid)
            {
                float prevLatitude = latitude;
                float prevLongitude = longitude;

                latitude = (minmea_tocoord(&frame.latitude));
                longitude = (minmea_tocoord(&frame.longitude));

                auto const relNorthrelEast = CoreUtils::northEastDistance(prevLatitude, prevLongitude, latitude, longitude);
                velocityNorth = relNorthrelEast.north;
                velocityEast = relNorthrelEast.east;

                groundSpeed = (minmea_tofloat(&frame.speed));
                // Course might not always be done and will result in a inf values in the filter
                if (frame.course.scale != 0)
                {
                    course(minmea_tofloat(&frame.course));
                }
                lastRMCTimestamp = frame.time;
                sendMessageWhenGGAisRMC();
            }
        }
    }
    break;

    case MINMEA_SENTENCE_GGA:
    {
        statistics.receivedGGA++;

        struct minmea_sentence_gga frame;
        if (minmea_parse_gga(&frame, msg.sentence.c_str()))
        {

            float height = convertToMeters(&frame.height, frame.height_units);
            if (height != INVALID_CONVERSION)
            {
                heightGeoidWGS84 = (height);
            }

            float alt = convertToMeters(&frame.altitude, frame.height_units);
            if (alt != INVALID_CONVERSION)
            {
                // Altitude from GPS is referenced to MSL, to get WGS96 offset needs to be added
                // https://www.unavco.org/software/geodetic-utilities/geoid-height-calculator/geoid-height-calculator.html
                altitudeWgs84(alt + (height != INVALID_CONVERSION ? height : 0));
            }

            lastGGATimestamp = frame.time;
            satellitesTracked = frame.satellites_tracked;
            // 0: Fix not valid
            // 1: GPS fix
            // 2: Differential GPS fix (DGNSS), SBAS, OmniSTAR VBS, Beacon, RTX in GVBS mode
            // 3: Not applicable
            // 4: RTK Fixed, xFill
            // 5: RTK Float, OmniSTAR XP/HP, Location RTK, RTX
            // 6: INS Dead reckoning
            fixQuality = frame.fix_quality;

            sendMessageWhenGGAisRMC();
        }
    }
    break;

    case MINMEA_SENTENCE_GSA:
    {
        statistics.receivedGSA++;

        if (gpsStatsMsg.isItTime(CoreUtils::timeUs32()))
        {
            struct minmea_sentence_gsa frame;
            if (minmea_parse_gsa(&frame, msg.sentence.c_str()))
            {
                pDop = minmea_tofloat(&frame.pdop);
                auto hDop = minmea_tofloat(&frame.hdop);
                getBus().receive(
                    OpenAce::GpsStatsMsg{
                        fixQuality,
                        (uint8_t)frame.fix_type,
                        satellitesTracked,
                        pDop == NAN ? 100 : pDop,
                        hDop == NAN ? 100 : hDop});
            }
        }
    }
    break;

    default:
    {
        statistics.receivedOther++;
    }
    }
}

/**
 * Convert an minmea_float with altitude/height information in meters
 */
float GpsDecoder::convertToMeters(const struct minmea_float *value, char unit) const
{
    float meters = minmea_tofloat(value);
    if (meters==NAN) {
        return INVALID_CONVERSION;
    }
    switch (unit)
    {
    case 'M':
    case 'm':
        return meters;
    case 'F':
    case 'f':
        return meters * FT_TO_M;
    default:
        return INVALID_CONVERSION;
    }
}

/**
 * Send message when both GGA and RMC sentences are received
 */
void GpsDecoder::sendMessageWhenGGAisRMC()
{
    //  if ((lastGGATimestamp <=> lastRMCTimestamp) == 0) {

    // Send message over bus when both GGA and GMC sentences are received at the same time
    // It's required that both GGA and GMC sentences have the same timestamp in these cases
    // If this in practise is not happening, due to newer GPS systems position should be taken from latest RMC
    // so we take position acuracy over altitude/course
    // TODO: Reconsider
    if (lastGGATimestamp.microseconds == lastRMCTimestamp.microseconds && lastGGATimestamp.seconds == lastRMCTimestamp.seconds)
    {
        auto alt = altitudeWgs84();
        auto height = heightGeoidWGS84;

        // TODO: Can we get bank angle from turnrate?? https://aviation.stackexchange.com/questions/65628/what-is-the-formula-for-the-bank-angle-required-for-a-turn-in-line-abreast-forma
        getBus().receive(
            OpenAce::OwnshipPositionMsg{
                OpenAce::OwnshipPositionInfo{
                    .timestamp = CoreUtils::timeUs32(),
                    .airborne = groundSpeed > OpenAce::GROUNDSPEED_CONSIDERING_AIRBORN ? true : false, // airborne
                    .lat = latitude,
                    .lon = longitude,
                    .altitudeWgs84 = static_cast<int16_t>(alt),
                    .verticalSpeed = altitudeWgs84.perSecond(), // vertical speed
                    .groundSpeed = groundSpeed,               // Ground Speed
                    .course = course(),
                    .hTurnRate = course.perSecond(), // hTurnRate   // degrees per second
                    .velocityNorth = velocityNorth,
                    .velocityEast = velocityEast,
                    .heightEgm96 = static_cast<int16_t>(alt - height),
                    .geoidOffset = static_cast<int16_t>(height)}});
    }
}
