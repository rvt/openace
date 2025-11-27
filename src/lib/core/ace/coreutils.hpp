#pragma once

#include "constants.hpp"
#include <stdint.h>
#include <time.h>
#include <type_traits>
#include <inttypes.h>

#include "pico/time.h"

#include "messages.hpp"

#include "etl/string.h"

class CoreUtils
{

inline static __scratch_y("GatasMem_offsetTimeToAbsolute") uint64_t CoreUtils_offsetTimeToAbsolute = 0;
inline static __scratch_y("GatasMem_timeUs32PpsOffset") uint32_t CoreUtils_timeUs32PpsOffset = 0;

public:
    /**
     * Dump a buffer as a hexidecimal string for terminal output
     */
    static void printBuffer(etl::span<uint8_t> buffer, bool lf = false)
    {
        printf("Length(%d) ", static_cast<int>(buffer.size()));
        for (size_t i = 0; i < buffer.size(); ++i)
        {
            printf("0x%02X", buffer[i]);
            if (i + 1 < buffer.size())
            {
                printf(", ");
            }
        }
        if (lf)
        {
            printf("\n");
        }
    }

    template <typename T>
    static void printBufferHex(etl::span<T> buffer)
    {
        GATAS_ASSERT(std::is_integral<T>::value, "T must be an integral type");

        printf("Length(%d) ", static_cast<int>(buffer.size()));

        for (size_t i = 0; i < buffer.size(); ++i)
        {
            if constexpr (sizeof(T) == 1)
                printf("0x%02" PRIX8, static_cast<uint8_t>(buffer[i]));
            else if constexpr (sizeof(T) == 2)
                printf("0x%04" PRIX16, static_cast<uint16_t>(buffer[i]));
            else if constexpr (sizeof(T) == 4)
                printf("0x%08" PRIX32, static_cast<uint32_t>(buffer[i]));
            else
                printf("0x%X", static_cast<unsigned int>(buffer[i])); // fallback

            if (i + 1 < buffer.size())
                printf(", ");
        }
    }

    /**
     * Convert and timestamp to an uint32_t which is synced with GPS time such that at PPS the ms should reppresents (somewhere close) to a ms
     * eg: 45'453'010 = represents 10ms after PPS
     * \deprecated
     */
    static uint32_t timeToPositionTs(int8_t hours, int8_t minutes, int8_t seconds, int16_t microseconds)
    {
        return hours * 3600 + minutes * 60 + seconds + microseconds;
    }

    /**
     * Get a timestamp in us aligned on PPS so that for example45000 is at the exact pps
     * This timestamp monotonically increases from power up
     * this works together with isReached to measure time differences
     * Use this to measure short time differences of less then 71minutes
     * \sa isReached
     */
    __force_inline static uint32_t timeUs32()
    {
        return time_us_32() - CoreUtils_timeUs32PpsOffset;
    }

    __force_inline static uint32_t timeUs32Raw()
    {
        return time_us_32();
    }

    __force_inline static uint64_t timeUs64()
    {
        // time_us_64 and time_us_32 use the same hardware time, thus offset is also the same
        return time_us_64() - CoreUtils_timeUs32PpsOffset;
    }

    /**
     * Get a timestamp in ms
     * This timestamp monotonically increases from power up
     * \sa timeUs32
     */
    static uint32_t timeMs32()
    {
        return timeUs64() / 1'000;
    }
    /**
     * Get a timestamp in seconds
     * This timestamp monotonically increases from power up
     */
    static uint32_t timeS32()
    {
        return timeUs64() / 1'000'000;
    }

    /**
     * Calculate the time from referenceUs to us
     * If referenceUs is in the past, the result is negative, eg the event happened
     */
    __force_inline static int32_t usToReference(uint32_t referenceUs, uint32_t us = timeUs32())
    {
        auto diff = static_cast<int32_t>(referenceUs - us);
#if GATAS_DEBUG == 1
        if (diff < -15 * 60 * 1'000'000 || diff > 15 * 60 * 1'000'000)
        {
            // These Routines usToReferenceRaw/usToReference/usDiff/isUsReached/isUsReachedRaw/timeUs32/timeUs32Raw are designed
            // for short durations only, eg what will happen in the next second, or perhaps a few minutes. They are not suitable for
            // very long delays. They can only be used up to 30minutes maximum before counters start to wrap over (wrap over is at ~ 1.2hour)
            GATAS_VERIFY(false, "WARNING: Large references far in the future are not expected from usToReference, please check code.");
        }
#endif
        return diff;
    }

    __force_inline static int32_t usToReferenceRaw(uint32_t referenceUsRaw, uint32_t us = timeUs32Raw())
    {
        auto diff = static_cast<int32_t>(referenceUsRaw - us);
#if GATAS_DEBUG == 1
        if (diff < -15 * 60 * 1'000'000 || diff > 15 * 60 * 1'000'000)
        {
            // These Routines usToReferenceRaw/usToReference/usDiff/isUsReached/isUsReachedRaw/timeUs32/timeUs32Raw are designed
            // for short durations only, eg what will happen in the next second, or perhaps a few minutes. They are not suitable for
            // very long delays. They can only be used up to 30minutes maximum before counters start to wrap over (wrap over is at ~ 1.2hour)
            // This is for developers only
            GATAS_VERIFY(false, "WARNING: Large references far in the future are not expected from usToReferenceRaw, please check code.");
        }
#endif
        return diff;
    }

    static int32_t usDiff(uint32_t referenceUs, uint32_t us = timeUs32())
    {
        return etl::absolute(usToReference(referenceUs, us));
    }

    /**
     * Decide if the referenced time is reached
     * Use this to measure short time differences of less then 71minutes
     * It will properly handle wraparounds if the time is less than 35 minutes in difference
     * \sa timeUs32
     */
    __force_inline static bool isUsReached(uint32_t referenceUs, uint32_t us = timeUs32())
    {
        return usToReference(referenceUs, us) <= 0;
    }
    __force_inline static bool isUsReachedRaw(uint32_t referenceUs, uint32_t us = timeUs32Raw())
    {
        return usToReferenceRaw(referenceUs, us) <= 0;
    }

    /**
     * Must be called at high priority to set the PPS offset.
     * When offset is known, the correct time in us in reference to PPS can be calculated
     * @param offsetUs Offset in us to add to the current time_us_32 to align with PPS This can be used for software PPS adjustments
     */
    static void __time_critical_func(setPPS)(int32_t offsetUs)
    {
        CoreUtils_timeUs32PpsOffset = time_us_32() % 1'000'000 - offsetUs;
    }

    /**
     * Returns the current ms within the current second (based on PPS)
     * eg: a value of 119 means 119ms since PPS
     */
    static uint16_t msInSecond()
    {
        return (timeUs32() / 1'000) % 1'000;
    }

    static uint16_t usInSecond()
    {
        return (timeUs64()) % 1'000'000;
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
    static uint16_t msDelayToReference(uint16_t refMsInSecond, uint16_t ms = msInSecond())
    {
        if (refMsInSecond > ms)
        {
            return refMsInSecond - ms;
        }
        else
        {
            return 1'000 - ms + refMsInSecond;
        }
    }

    //////////////// EPOCH functions ////////////////
    /**
     * Set's the offset to the current time in ms since epoch.
     * THis function should be called to sync GPS time to the PICO's time using the PPS from the GPS
     * and is given the exact epochoffset when received from PPS this will calibrate the epoch function to exact ms
     * See RTC::on_receive(const GATAS::UtcTimeMsg& msg)
     */
    static void setOffsetMsSinceEpoch(uint64_t msSinceEpoch)
    {
        CoreUtils_offsetTimeToAbsolute = msSinceEpoch - time_us_64() / 1'000;
    }

    /**
     * Returns the current time in ms since epoch
     */
    static uint64_t msSinceEpoch()
    {
        return (time_us_64() / 1'000) + CoreUtils_offsetTimeToAbsolute;
    }
    /**
     * Seconds since EPOCH, like unix time
     */
    static uint32_t secondsSinceEpoch()
    {
        return msSinceEpoch() / 1000;
    }

    static tm localTime()
    {
        return localTime(msSinceEpoch());
    }

    static tm localTime(uint64_t msSinceEpoch)
    {
        time_t secondsSinceEpoch = msSinceEpoch / 1000;
        struct tm timeinfo = {};
        localtime_r(&secondsSinceEpoch, &timeinfo);
        return timeinfo;
    }
    //////////////// EPOCH functions ////////////////

    /**
     * Get the current time in hours, minutes and seconds since mightnight UTC based on epoch
     */
    static auto hourMinutesSeconds(uint32_t now = secondsSinceEpoch())
    {
        struct HourMinuteSeconds
        {
            uint8_t hour;
            uint8_t minute;
            uint8_t second;
        };

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

    static auto northEastDistance(float fromLat, float fromLon, float toLat, float toLon)
    {
        struct relNorthRllEast
        {
            float north;
            float east;
        };

        float kx = cosf(fromLat * DEG_TO_RADS) * 111321;
        float dx = (toLon - fromLon) * kx;
        float dy = (toLat - fromLat) * 111139;
        return relNorthRllEast{dy, dx};
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
        auto ne = northEastDistance(fromLat, fromLon, toLat, toLon);
        return sqrtf((ne.north * ne.north) + (ne.east * ne.east));
    }

    /**
     * Calculate the bearing between two points on earth
     * returns bearing in degrees 0≤x<360
     * For short distances you can also use bearingFromInDegShort
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
     * A faster method than using bearingFromInRad that works with the relative coordinates we have to calculate anyways
     * This works for shorter distances well but cannot be used for long distances (>1000Km)
     */
    static float __time_critical_func(bearingFromInDegShort)(float east, float north)
    {
        float theta = atan2f(east, north);
        float deg = theta * 180.f / M_PI;
        if (deg < 0.f)
            deg += 360.f;
        return deg;
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
        uint16_t bearing()
        {
            auto bearing = bearingFromInDegShort(relEast, relNorth);
            if (bearing >= 360)
            {
                bearing = 0;
            }
            return bearing;
        }
        //        int16_t bearing;
    };
    struct distanceRelNorthRelEastFloat
    {
        float distance;
        float relNorthMeter;
        float relEastMeter;
        float bearing()
        {
            return bearingFromInDegShort(relEastMeter, relNorthMeter);
        }

        //        float bearing;
    };

    static distanceRelNorthRelEastInt getDistanceRelNorthRelEastInt(const GATAS::AircraftPositionInfo &from, const GATAS::AircraftPositionInfo &to)
    {
        return getDistanceRelNorthRelEastInt(from.lat, from.lon, to.lat, to.lon);
    }

    static distanceRelNorthRelEastInt getDistanceRelNorthRelEastInt(const GATAS::OwnshipPositionInfo &from, const GATAS::AircraftPositionInfo &to)
    {
        return getDistanceRelNorthRelEastInt(from.lat, from.lon, to.lat, to.lon);
    }

    /**
     * Calculate the bearing and ensures it's between 0..360
     */
    template <typename T>
    constexpr static T toBearing(T angle)
    {
        while (angle < static_cast<T>(0))
            angle += static_cast<T>(360);

        while (angle >= static_cast<T>(360))
            angle -= static_cast<T>(360);

        return angle;
    }

    /**
     * Calculates the relative North and East + Bearing of two coordinates
     *
     * Note: This function takea around 75us..100us to complete
     */
    static distanceRelNorthRelEastInt __time_critical_func(getDistanceRelNorthRelEastInt)(float fromLat, float fromLon, float toLat, float toLon)
    {
        auto drne = getDistanceRelNorthRelEastFloat(fromLat, fromLon, toLat, toLon);

        // int16_t bearing = drne.bearing + 0.5f;
        // if (bearing >= 360)
        // {
        //     bearing = 0;
        // }
        return {
            static_cast<uint32_t>(drne.distance + 0.5f),
            static_cast<int32_t>(drne.relNorthMeter + 0.5f),
            static_cast<int32_t>(drne.relEastMeter + 0.5f),
        };
    }

    /**
     * From two lat/lon points calculate relativeNorth/relativeEast distance and bearing between the two points
     */
    static distanceRelNorthRelEastFloat __time_critical_func(getDistanceRelNorthRelEastFloat)(float fromLat, float fromLon, float toLat, float toLon)
    {
        auto ne = northEastDistance(fromLat, fromLon, toLat, toLon);
        // float bearing = bearingFromInDegShort(ne.east, ne.north);
        float distance = sqrtf((ne.north * ne.north) + (ne.east * ne.east));
        return {distance, ne.north, ne.east};
    }

    /**
     * Parse an path in the form of /foo/bar/bas.extension
     * returns a vector with foo, bar, bas, extension
     */
    static const etl::vector<GATAS::Modulename, 7> parsePath(const etl::string_view path, const etl::string_view key = "");

    /**
     * Devide a circle in a number of sections.
     * The sections is devided such that radial zero falls from -XX to XX where XX = M_PI/sections, section 1 XX..2XX etc
     * The resultng value is always 0..(SECTIONS-1)
     */
    template <int SECTIONS>
    static int getRadialSection(int16_t degree)
    {
        constexpr int16_t sectionSize = 360 / SECTIONS;

        // Calculate the section
        return static_cast<int>(fmodf((degree + (sectionSize >> 1)) / sectionSize, SECTIONS));
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
    static int8_t egmGeoidOffset(float lat, float lon);

    /**
     * Add's the checksum and postfix characters to a NMEA string. It may contain an existing checksum that will be overwritten
     * When the capacity is not enough, the result is undefined
     * Note: Must start with the prefix character $
     * @param nmea example '$PFEC,GPint,RMC05'
     * @return             '$PFEC,GPint,RMC05*2D\r\n'
     */
    static void addChecksumToNMEA(etl::istring &nmea)
    {
        const char hexChars[] = "0123456789ABCDEF";
        uint16_t chk = 0, i = 1;
        while (nmea[i] && nmea[i] != '*')
        {
            chk ^= nmea[i];
            i += 1;
        }

        if (i > (nmea.capacity() - 5))
        {
            return;
        }
        nmea.resize(i);

        char checksumSuffix[] = {
            '*',
            hexChars[(chk >> 4) & 0x0F],
            hexChars[chk & 0x0F],
            '\r',
            '\n',
        };

        nmea.append(checksumSuffix, 5);
    }

    template <size_t Array_Size>
    static etl::string<Array_Size + 6> createNmeaChecksum(const char (&text)[Array_Size])
    {
        etl::string<Array_Size + 6> nmea(text, etl::strlen(text, Array_Size - 1));
        addChecksumToNMEA(nmea);
        return nmea;
    }

    static uint32_t getTotalHeap(void);
    static uint32_t getFreeHeap(void);

    /**
     * Create an textual representation of the aircraftId. FOr the moment it will simply turn the aircraftID as received into a textual HEX representation
     * Later the idea is that it will use DDB to get the registration based on aircraftID and addressType
     *
     */
    static void streamIcaoAddress(etl::string_stream &stream, uint32_t aircraftID, GATAS::AddressType addressType, etl::string_view callSign = "")
    {
        (void)addressType;
        stream << etl::hex << etl::setw(6) << etl::setfill('0') << etl::uppercase << aircraftID << GATAS::RESET_FORMAT;
        if (callSign.size() > 0)
        {
            stream << "!" << callSign;
        }
    }

    static uint8_t getHexVal(char hex)
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
    static void hexStrToByteArray(const char hex[], uint8_t hexLength, uint8_t bytearray[])
    {
        for (uint8_t i = 0, j = 0; i < hexLength; i += 2, ++j)
        {
            bytearray[j] = (getHexVal(hex[i]) << 4) | getHexVal(hex[i + 1]);
        }
    }

    static void hexStrToByteArray(const char *hex, uint8_t byteArray[])
    {
        auto hexLength = strlen(hex);
        hexStrToByteArray(hex, hexLength, byteArray);
    }

    /**
     * Convert a byteArray to a hex string, the reverse of hexStrToByteArray
     */
    static void byteArrayToHexStr(const uint8_t byteArray[], uint8_t byteArrayLength, char hexStr[])
    {
        const char hexChars[] = "0123456789ABCDEF";
        for (uint8_t i = 0; i < byteArrayLength; ++i)
        {
            hexStr[i * 2] = hexChars[(byteArray[i] >> 4) & 0x0F]; // Extract the upper 4 bits
            hexStr[i * 2 + 1] = hexChars[byteArray[i] & 0x0F];    // Extract the lower 4 bits
        }
        hexStr[byteArrayLength * 2] = '\0'; // Null-terminate the string
    }

    /**
     * Returns the pin number from the pin map, when not found returns -1 to indicate that
     */
    static int8_t pinValue(const GATAS::PinTypeMap &pm, const GATAS::PinType &pinName, int8_t defaultValue = -1)
    {
        auto it = pm.find(pinName);
        if (it != pm.end())
        {
            return it->second;
        }
        return defaultValue;
    }

    /**
     * Decide if an aircraft is airborn based on category and speed
     * Own our ownship is a Helicopter or baloon, it's best to indicate we are airborn, even if we don't have an airspeed
     * This is the same for obstacle or slow moving objects. Some EFB's don't show traffic on 'ground', so to ensure
     * they are indicated, airborn will be set to true
     */
    static bool isAirborn(GATAS::AircraftCategory cat, float speedMs)
    {
        switch (cat)
        {
        case GATAS::AircraftCategory::LIGHT_THAN_AIR:
        case GATAS::AircraftCategory::SKY_DIVER:
        case GATAS::AircraftCategory::ROTORCRAFT:
        case GATAS::AircraftCategory::UNKNOWN:
        case GATAS::AircraftCategory::UN_MANNED:
        case GATAS::AircraftCategory::SURFACE_EMERGENCY_VEHICLE:
        case GATAS::AircraftCategory::SURFACE_VEHICLE:
        case GATAS::AircraftCategory::POINT_OBSTACLE:
        case GATAS::AircraftCategory::CLUSTER_OBSTACLE:
        case GATAS::AircraftCategory::LINE_OBSTACLE:
        case GATAS::AircraftCategory::HANG_GLIDER:
        case GATAS::AircraftCategory::PARA_GLIDER:
            return true;
        default:
            return speedMs > GATAS::GROUNDSPEED_CONSIDERING_AIRBORN; // 10 m/s threshold for others
        }
    }
};
