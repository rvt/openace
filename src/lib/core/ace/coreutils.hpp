#pragma once

#include "constants.hpp"
#include <stdint.h>
#include <time.h>

#include "pico/time.h"

#include "messages.hpp"
#include "coreutils.hpp"

#include "etl/string.h"
#include "etl/string_utilities.h"

class CoreUtils
{
    inline static uint64_t offsetTimeToAbsolute;

public:
    /**
     * Convert and timestamp to an OpenAce::positionTs which is synced with GPS time such that at PPS the ms should reppresents (somewhere close) to a ms
     * eg: 45'453'010 = represents 10ms after PPS
     * \deprecated
     */
    static inline OpenAce::positionTs timeToPositionTs(int8_t hours, int8_t minutes, int8_t seconds, int16_t microseconds)
    {
        return hours * 3600 + minutes * 60 + seconds + microseconds;
    }

    /**
     * Return a position timestamp.
     * Use this to keep track on traffic and when it was received.
     * it currently uses it's own type but us currently driven from msSinceEpoch();
     */
    static inline OpenAce::positionTs getPositionTs()
    {
        return msSinceEpoch();
    }

    /**
     * Get a timestamp in us
     * This timestamp monotonically increases from power up
     * this works together with isReached to measure time differences
     * Use this to measure short time differences of less then 71minutes
     * \sa isReached
     */
    static inline uint32_t timeUs32()
    {
        return time_us_32();
    }
    /**
     * Get a timestamp in ms
     * This timestamp monotonically increases from power up
     * Note: When measuring time differences up to 35 minutes, it's best to use timeUs32 for performance reasons
     * \sa timeUs32
     */
    static inline uint32_t timeMs32()
    {
        return time_us_64() / 1'000;
    }
    /**
     * Get a timestamp in seconds
     * This timestamp monotonically increases from power up
     */
    static inline uint32_t timeS32()
    {
        return time_us_64() / 1'000'000;
    }

    /**
     * Decide if the referenced time is reached
     * Use this to measure short time differences of less then 71minutes
     * It will properly handle wraparounds if the time is less than 35 minutes in difference
     * \sa timeUs32
     */
    static inline bool isUsReached(uint32_t referenceUs)
    {
        return ((uint32_t)(timeUs32() - referenceUs)) < (uint32_t)(0x80000000);
    }

    /**
     * Decide if the reference time is reached
     * Use this to measure short time differences of less then 71minutes
     * It will properly handle wraparounds if the time is less than 35 minutes in difference
     * \sa timeMs32
     */
    static inline bool isMsReached(uint32_t referenceMs)
    {
        return ((uint32_t)(timeMs32() - referenceMs)) < (uint32_t)(0x80000000);
    }

    /**
     * Calculate the number if us Elapsed from a reference
     * Use this for timing short time differences
     * \sa: referenceUs is the time taken
     * \sa: Is the new, later time
     */
    static inline uint32_t usElapsed(uint32_t referenceUs, uint32_t usBoot = CoreUtils::timeUs32())
    {
        return usBoot - referenceUs;
    }

    /**
     * Set's the offset to the current time in ms since epoch.
     * THis function should be called to sync GPS time to the PICO's time using the PPS from the GPS
     * and is given the exact epochoffset when received from PPS this will calibrate the epoch function to exact ms
     * See RTC::on_receive(const OpenAce::GpsTime& msg)
     */
    static inline void setOffsetMsSinceEpoch(uint64_t msSinceEpoch)
    {
        CoreUtils::offsetTimeToAbsolute = msSinceEpoch - time_us_64() / 1000;
    }

    /**
     * Returns the current time in ms since epoch
     */
    static inline uint64_t msSinceEpoch()
    {
        return (time_us_64() / 1000) + CoreUtils::offsetTimeToAbsolute;
    }

    /**
     * Returns the current ms within teh current second (sinced to epoch)
     * eg: a value of 119 means 119ms since PPS
     */
    static inline uint16_t msInSecond(uint64_t msSinceEpoch = CoreUtils::msSinceEpoch())
    {
        return msSinceEpoch % 1000;
    }

    /**
     * Calculate the time in ms to the next refMsInSecond time
     * Example: current time in msInSecond is 600
     * When reference time is 700, the result is 100
     * When reference time is 200, the result is 600 (Wraps over a whole second)
     * Currently will only allow to delay for 1000ms max
     * ms needs to be 0..1000
     * refMsInSecond 0..n
     */
    static inline uint16_t msDelayToReference(uint16_t refMsInSecond, uint16_t ms = msInSecond())
    {
        if (refMsInSecond > ms)
        {
            return refMsInSecond - ms;
        }
        else
        {
            return 1000 - ms + refMsInSecond;
        }
    }

    /**
     * Seconds since EPOCH, like unix time
     */
    static inline uint32_t secondsSinceEpoch()
    {
        return msSinceEpoch() / 1000;
    }

    static inline tm localTime()
    {
        return localTime(msSinceEpoch());
    }
    static inline tm localTime(uint64_t msSinceEpoch)
    {
        time_t secondsSinceEpoch = msSinceEpoch / 1000;
        struct tm timeinfo = {};
        localtime_r(&secondsSinceEpoch, &timeinfo);
        return timeinfo;
    }

    struct HourMinuteSeconds
    {
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
    };

    /**
     * Get the current time in hours, minutes and seconds since mightnight UTC based on epoch
     */
    static inline HourMinuteSeconds hourMinutesSeconds(uint32_t now = secondsSinceEpoch())
    {

        uint8_t h = (uint8_t)((now / 3600) % 24);
        uint8_t m = (uint8_t)((now / 60) % 60);
        uint8_t s = (uint8_t)(now % 60);

        return HourMinuteSeconds{h, m, s};
    }

    /**
     * Calculate distance between two points on earth using haversine
     * For a fast method check distanceFast and getDistanceRelNorthRelEastFloat
     *
     * returns distance in meters
     * Reference: https://geopy.readthedocs.io/en/stable/#module-geopy.distance
     * TODO: Test performance with https://www.eevblog.com/forum/microcontrollers/lightweight-sin(2x)-cos(2x)-for-mcus-with-float-hardware/ ??
     */
    static float distanceAccurate(float fromLat, float fromLon, float toLat, float toLon)
    {
        fromLat *= DEG_TO_RADS;
        fromLon *= DEG_TO_RADS;
        toLat *= DEG_TO_RADS;
        toLon *= DEG_TO_RADS;
        float a = sinf(0.5f * (toLat - fromLat));
        float b = sinf(0.5f * (toLon - fromLon));
        float c = a * a + cosf(fromLat) * cosf(toLat) * b * b;
        float d = 2.f * atan2f(sqrtf(c), sqrtf(fabsf(1.f - c)));
        return d * DIAMETER_EARTH_M;
    }

    struct relNorthRllEast
    {
        float north;
        float east;
    };
    static relNorthRllEast northEastDistance(float fromLat, float fromLon, float toLat, float toLon)
    {
        float kx = cosf(fromLat * DEG_TO_RADS) * 111321;
        float dx = (fromLon - toLon) * kx;
        float dy = (fromLat - toLat) * 111139;
        return {dy, dx};
    }

    /**
     * Calculate the distance between two points on earth fast but less accurate
     * For accurate values use distanceAccurate
     *
     * Note: Distance in what we are interested in <30Km at lat 30 degrees the difference is about 15m
     *
     * returns distance in meters
     * https://jamesloper.com/fastest-way-to-calculate-distance-between-two-coordinates
     * https://www.movable-type.co.uk/scripts/latlong.html
     */
    static float distanceFast(float fromLat, float fromLon, float toLat, float toLon)
    {
        auto const &ne = northEastDistance(fromLat, fromLon, toLat, toLon);
        return sqrtf((ne.north * ne.north) + (ne.east * ne.east));
    }

    /**
     * Calculate the bearing between two points on earth
     * returns bearing in degrees 0≤x<360
     * https://www.movable-type.co.uk/scripts/latlong.html
     */
    static float bearingFromInRad(float fromLat, float fromLon, float toLat, float toLon)
    {
        fromLat *= DEG_TO_RADS;
        fromLon *= DEG_TO_RADS;
        toLat *= DEG_TO_RADS;
        toLon *= DEG_TO_RADS;
        float dLon = toLon - fromLon;
        float cosToLat = cosf(toLat);
        float bearingDegrees = atan2f(sinf(dLon) * cosToLat, (cosf(fromLat) * sinf(toLat)) - (sinf(fromLat) * cosToLat * cosf(dLon)));
        return fmodf((bearingDegrees + M_TWOPI), M_TWOPI);
    }

    /**
     * Same as bearingFrom but returns in degrees
     */
    static float bearingFromInDeg(float fromLat, float fromLon, float toLat, float toLon)
    {
        return bearingFromInRad(fromLat, fromLon, toLat, toLon) * RADS_TO_DEG;
    }

    /**
     * Get the relative North, Eats and distance from a position to a position
     * Distance returned is distance inmeters and take in considation the altitude
     * All values returned are in meters
     * @param from Position where to calculate baring from
     * @param to Position where to calculate baring to
     * @param distance Distance in meters over ground
     */

    struct distanceRelNorthRelEastInt
    {
        uint32_t distance;
        int32_t relNorth;
        int32_t relEast;
        int16_t bearing;
    };
    struct distanceRelNorthRelEastFloat
    {
        float distance;
        float relNorthMeter;
        float relEastMeter;
        float bearing;
    };

    inline static distanceRelNorthRelEastInt getDistanceRelNorthRelEastInt(const OpenAce::AircraftPositionInfo &from, const OpenAce::AircraftPositionInfo &to)
    {
        return getDistanceRelNorthRelEastInt(from.lat, from.lon, to.lat, to.lon);
    }

    inline static distanceRelNorthRelEastInt getDistanceRelNorthRelEastInt(const OpenAce::OwnshipPositionInfo &from, const OpenAce::AircraftPositionInfo &to)
    {
        return getDistanceRelNorthRelEastInt(from.lat, from.lon, to.lat, to.lon);
    }

    /**
     * Calculate the bearing and ensures it's between 0..360
     */
    template <typename T>
    static T toBearing(T angle)
    {
        while (angle < static_cast<T>(0))
            angle += static_cast<T>(360);

        while (angle >= static_cast<T>(360))
            angle -= static_cast<T>(360);

        return angle;
    }

    static distanceRelNorthRelEastInt getDistanceRelNorthRelEastInt(float fromLat, float fromLon, float toLat, float toLon)
    {
        auto const &drne = getDistanceRelNorthRelEastFloat(fromLat, fromLon, toLat, toLon);
        // float fbearing = CoreUtils::bearingFromInRad(fromLat, fromLon, toLat, toLon);
        // float fdistance = CoreUtils::distanceFast(fromLat, fromLon, toLat, toLon);
        // int32_t relNorth = static_cast<int32_t>(cosf(fbearing) * fdistance + 0.5f);
        // int32_t relEast = static_cast<int32_t>(sinf(fbearing) * fdistance + 0.5f);
        // int16_t bearing = static_cast<int16_t>((fbearing * RADS_TO_DEG) + 0.5f);
        // int32_t distance = static_cast<int32_t>(fdistance);

        int16_t bearing = toBearing(static_cast<int16_t>(drne.bearing + 0.5f));

        return {static_cast<uint32_t>(drne.distance + 0.5f),
                static_cast<int32_t>(drne.relNorthMeter + 0.5f),
                static_cast<int32_t>(drne.relEastMeter + 0.5f),
                bearing};
    }

    /**
     * From two lat/lon points calculate relativeNorth/relativeEast distance and bearing between the two points
     */
    static distanceRelNorthRelEastFloat getDistanceRelNorthRelEastFloat(float fromLat, float fromLon, float toLat, float toLon)
    {
        float fbearing = CoreUtils::bearingFromInRad(fromLat, fromLon, toLat, toLon);
        float fdistance = CoreUtils::distanceFast(fromLat, fromLon, toLat, toLon);
        float relNorth = cosf(fbearing) * fdistance;
        float relEast = sinf(fbearing) * fdistance;
        float bearing = fbearing * RADS_TO_DEG;
        return {fdistance, relNorth, relEast, bearing};
    }

    /**
     * Parse an path in the form of /foo/bar/bas.extension
     * returns a vector with foo, bar, bas, extension
     */
    static const etl::vector<OpenAce::Modulename, 7> parsePath(const etl::string_view path);

    /**
     * Devide a circle in a number of sections.
     * The sections is devided such that radial zero falls from -XX to XX where XX = M_PI/sections, section 1 XX..2XX etc
     * The resultng value is always 0..(SECTIONS-1)
     */
    template <int SECTIONS>
    static int getRadialSection(int16_t degree)
    {
        int16_t sectionSize = 360 / SECTIONS;

        // Calculate the section
        return fmodf((degree + (sectionSize >> 1)) / sectionSize, SECTIONS);
    }

    template <int SECTIONS>
    static int getRadialSectionRad(float rad)
    {
        return getRadialSection<SECTIONS>(rad * RADS_TO_DEG);
    }

    /*
     * taken from XCSoar
     * Get's the Geiod of a specific location, based on a simple lookup table
     * See also : https://geographiclib.sourceforge.io/cgi-bin/GeoidEval
     * Note: Only use of the GPS with GGA does nit send the egm96 data
     */
    static int8_t egm96GeoidOffset(float lat, float lon);

    /**
     * Add's the checksum and postfix characters to a NMEA string. It may contain an existing checksum that will be overwritten
     * @param nmea example '$PFEC,GPint,RMC05*XX'
     * @return example '$PFEC,GPint,RMC05*2D\r\n'
     */
    static void addChecksumToNMEA(etl::istring &nmea)
    {
        const char hexChars[] = "0123456789ABCDEF";
        uint16_t chk = 0, i = 1;
        while (nmea[i] && nmea[i] != '*')
        {
            chk ^= nmea[i];
            i++;
        }

        if (i > (nmea.capacity() - 6))
        {
            return;
        }
        nmea.resize(i + 5);
        nmea[i++] = '*';
        nmea[i++] = hexChars[chk >> 4];
        nmea[i++] = hexChars[chk & 0x0F];
        nmea[i++] = '\r';
        nmea[i++] = '\n';
        nmea[i++] = '\0';
    }

    static uint32_t getTotalHeap(void);
    static uint32_t getFreeHeap(void);
};

inline uint8_t getHexVal(char hex)
{
    uint8_t val = (uint8_t)hex;
    // For uppercase A-F letters:
    // return val - (val < 58 ? 48 : 55);
    // For lowercase a-f letters:
    // return val - (val < 58 ? 48 : 87);
    // Or the two combined, but a bit slower:
    return val - (val < 58 ? 48 : (val < 97 ? 55 : 87));
}

/**
 * Convert a hex string in the form of 0123FA... to a byte array
 */
inline void hexStrToByteArray(const char hex[], uint8_t hexLength, uint8_t byteArray[])
{
    for (uint8_t i = 0, j = 0; i < hexLength; i += 2, ++j)
    {
        byteArray[j] = (getHexVal(hex[i]) << 4) | getHexVal(hex[i + 1]);
    }
}

inline void hexStrToByteArray(const char *hex, uint8_t byteArray[])
{
    hexStrToByteArray(hex, strlen(hex), byteArray);
}

/**
 * Convert a byteArray to a hex string, the reverse of hexStrToByteArray
 */
inline void byteArrayToHexStr(const uint8_t byteArray[], uint8_t byteArrayLength, char hexStr[])
{
    const char hexChars[] = "0123456789ABCDEF";
    for (uint8_t i = 0; i < byteArrayLength; ++i)
    {
        hexStr[i * 2] = hexChars[(byteArray[i] >> 4) & 0x0F]; // Extract the upper 4 bits
        hexStr[i * 2 + 1] = hexChars[byteArray[i] & 0x0F];    // Extract the lower 4 bits
    }
    hexStr[byteArrayLength * 2] = '\0'; // Null-terminate the string
}
