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
    stream << ",\"gpsRatePerSec\":" << getGpsRate();
    stream << ",\"fixQuality\":" << fixQuality;
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
    static Every<uint32_t, 10'000'000, 30'000'000> gpsStatsMsg{0}; // Every 30 seconds 10 seconds in

    switch (minmea_sentence_id(msg.sentence.c_str(), false))
    {
    case MINMEA_SENTENCE_RMC:
    {
        static bool forceSendTime = true;
        statistics.receivedRMC++;
        struct minmea_sentence_rmc frame;
        if (minmea_parse_rmc(&frame, msg.sentence.c_str()))
        {
            uint16_t millis = frame.time.microseconds / 1000;

#ifndef NDEBUG
            // Print the time difference between the RMC and the local time
            // It's required to have this within a second accurate since some protocols like FLARM
            // depends on high accurate seconds since EPOCH
            auto msSinceEpoch = static_cast<uint32_t>(CoreUtils::msSinceEpoch() % 1000);
            auto secondsSinceEpoch = CoreUtils::secondsSinceEpoch();
            auto msInSecond = CoreUtils::msInSecond();

            time_t t = (time_t)secondsSinceEpoch;
            struct tm *timeinfo = localtime(&t); // or gmtime() for UTC

            if ((timeinfo->tm_hour != frame.time.hours ||
                 timeinfo->tm_min != frame.time.minutes ||
                 timeinfo->tm_sec != frame.time.seconds) &&
                CoreUtils::secondsSinceEpoch() > 1000000000)
            {

                printf("CoreUtils::secondsSinceEpoch(): %02d:%02d:%02d.%03ld RMC:%02d:%02d:%02d.%03d\n",
                       timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, msSinceEpoch,
                       frame.time.hours, frame.time.minutes, frame.time.seconds, msInSecond);
            }
#endif

            // Send only 2 times per minute
            if ((frame.time.seconds == 0 || frame.time.seconds == 30 || forceSendTime) && millis == 0 )
            {
                forceSendTime = fixType == 0;

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
                if (frame.latitude.scale == 0 || frame.longitude.scale == 0)
                {
                    return;
                }

                float prevLatitude = latitude;
                float prevLongitude = longitude;

                latitude = (minmea_tocoord(&frame.latitude));
                longitude = (minmea_tocoord(&frame.longitude));

                auto const relNorthrelEast = CoreUtils::northEastDistance(prevLatitude, prevLongitude, latitude, longitude);
                velocityNorth = relNorthrelEast.north;
                velocityEast = relNorthrelEast.east;

                groundSpeed = getFloat(frame.speed, groundSpeed) * KN_TO_MS; // Groundspeed in knt???
                // Course might not always be available and will result in a inf values in the filter
                if (frame.course.scale != 0)
                {
                    course(getFloat(frame.course, course()));
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

            heightGeoidWGS84 = convertToMeters(frame.height, frame.height_units, heightGeoidWGS84);
            altitude = convertToMeters(frame.altitude, frame.height_units, altitude);

            // https://www.unavco.org/software/geodetic-utilities/geoid-height-calculator/geoid-height-calculator.html
            altitudeWgs84(altitude + heightGeoidWGS84);

            satellitesTracked = frame.satellites_tracked;
            fixQuality = frame.fix_quality;
            lastGGATimestamp = frame.time;

            sendMessageWhenGGAisRMC();
        }
    }
    break;

    case MINMEA_SENTENCE_GSA:
    {
        statistics.receivedGSA++;

        if (gpsStatsMsg.isItTime(CoreUtils::timeUs32Raw()))
        {
            struct minmea_sentence_gsa frame;
            if (minmea_parse_gsa(&frame, msg.sentence.c_str()))
            {
                fixType = (uint8_t)frame.fix_type;
                pDop = getFloat(frame.pdop, 100);
                auto hDop = getFloat(frame.hdop, 100);
                getBus().receive(
                    OpenAce::GpsStatsMsg{
                        fixQuality,              // 0:Fix Not Valid 1:GPS fix 2:DGPS SBAS etc.. 3..6:NA
                        (uint8_t)frame.fix_type, // 1:NA 2:2D 3:3D
                        satellitesTracked,
                        pDop,
                        hDop});
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
float GpsDecoder::convertToMeters(const minmea_float &f, char unit, float defaultValue) const
{
    if (f.scale == 0)
    {
        return defaultValue;
    }
    float meters = minmea_tofloat(&f);
    switch (unit)
    {
    case 'M':
    case 'm':
        return meters;
    case 'F':
    case 'f':
        return meters * FT_TO_M;
    default:
        return defaultValue;
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
                    .groundSpeed = groundSpeed,                 // Ground Speed
                    .course = course(),
                    .hTurnRate = course.perSecond(), // hTurnRate   // degrees per second
                    .velocityNorth = velocityNorth,
                    .velocityEast = velocityEast,
                    .heightEgm96 = static_cast<int16_t>(alt - height),
                    .geoidOffset = static_cast<int16_t>(height)}});
    }
}
