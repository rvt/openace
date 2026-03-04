#include <stdio.h>

#include "../L76B.hpp"

// *INDENT-OFF*
// clang-format off
// https://files.waveshare.com/upload/5/56/Quectel_L76_Series_GNSS_Protocol_Specification_V3.3.pdf
// Wavesgare seems to have the L76B 3333 platform
// This is important for PPS/NMEA sync THis is what I get: $PMTK705,AXN_5.1.6_3333_18060500,0001,Quectel-L76B,1.0*53  -> FOUND
static auto BAUDRATE_115200 = CoreUtils::createNmeaChecksum("$PMTK251,115200");
static auto WARMSTART = CoreUtils::createNmeaChecksum("$PMTK102");
static auto HOTRESTART = CoreUtils::createNmeaChecksum("$PMTK101");
static auto HZ2_500 = CoreUtils::createNmeaChecksum("$PMTK220,500");
static auto HZ2_1000 = CoreUtils::createNmeaChecksum("$PMTK220,1000");
static auto CONSTELLATIONS = CoreUtils::createNmeaChecksum("$PMTK353,1,1,1,0,0"); // Must be GPS, Glonas and GAlileo only 
static auto SBAS = CoreUtils::createNmeaChecksum("$PMTK313,1");     // Enable SBAS
static auto SBASDGPS = CoreUtils::createNmeaChecksum("$PMTK301,2"); // Enable SBAS DGPS corrections
static auto SENTENCES = CoreUtils::createNmeaChecksum("$PMTK314,0,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
static auto NOSENTENCES = CoreUtils::createNmeaChecksum("$PMTK314,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
static auto PPS3DFIX = CoreUtils::createNmeaChecksum("$PMTK285,4,50"); // Use 1 to always output PPS, even without a fix
static auto NOPOWERSAVE = CoreUtils::createNmeaChecksum("$PMTK225,0"); // Normal mode
static auto VERSION = CoreUtils::createNmeaChecksum("$PMTK605");
static auto VERSIONNO= CoreUtils::createNmeaChecksum("$PQVERNO,R"); 
static auto ENABLEAIC = CoreUtils::createNmeaChecksum("$PMTK286,1");
static auto STATICNAVTHD= CoreUtils::createNmeaChecksum("$PMTK386,1"); // 3.6Km/h



// 255 – Sync 1PPS and NMEA Messages must be turned off because we run the GPS at a higher frequency
// Note to other developers: It looks like that the L76B in SOftware PPS mode is accurate only once 
// it has a proper 3D fix with more than 5 sats Where I see a delay of 240ish ms from PPS to RMC
static auto PPSNMEA = CoreUtils::createNmeaChecksum("$PMTK255,1");

// clang-format on
// *INDENT-ON*

void L76B::popGPSMessages(etl::string_view waitFor)
{
    constexpr uint16_t LONG_WAIT = 1500;
    constexpr uint16_t SHORT_WAIT = 500;

    GATAS::NMEAString sentence;
    int c = waitFor.length() > 0 ? LONG_WAIT : SHORT_WAIT;
    while (c--)
    {
        while (popQueue(sentence))
        {
            if (sentence.starts_with("$P"))
            {
                GATAS_INFO("%s ", sentence.c_str());
                if (waitFor.length() > 0 && sentence.starts_with(waitFor))
                {
                    GATAS_INFO(" -> FOUND");
                    c = 0;
                }
                else
                {
                    GATAS_INFO("");
                }
            }
        }
        vTaskDelay(TASK_DELAY_MS(10));
    }
}

// Note on PPS:
// L76B uses rising pulse to trigger PPS
// https://www.researchgate.net/figure/PPS-and-NMEA-timing-for-L76-M33_fig11_318528181
bool L76B::configureGnss()
{
    // Initialise the GPS hardware
    setStatus("Search");
    uint32_t baudrate = getSerial().findBaudRate(1'000);
    if (!baudrate)
    {
        setStatus("NO GPS");
        return false;
    }
    setStatusBaud(baudrate);

    // If not correct, try to set the GPS to the required baudrate
    if (baudrate != REQUIRED_GPS_BAUDRATE)
    {
        setStatus("Found");
        // printf("GPS found at %ldBd setting to %ldBd, waiting for GPS to come back on... ", baudrate, GPS_BAUDRATE);
        if (!getSerial().enableTx(baudrate))
        {
            return false;
        }

        getSerial().sendBlocking(BAUDRATE_115200);
        vTaskDelay(TASK_DELAY_MS(5000));
        getSerial().sendBlocking(HOTRESTART);
        popGPSMessages("$PMTK011,MTKGPS");
        return false;
    }

    if (!getSerial().enableTx(baudrate))
    {
        setStatus("Cfg err m8nCfg");
        setStatusBaud(0);
        return false;
    }

    GATAS_INFO("Starting L76B configuration");

    // Ensure queue is empty
    popGPSMessages();

    getSerial().sendBlocking(VERSION);
    popGPSMessages("$PMTK705");

    // According documentation NMEA Sync only works with 1HZ NMEA?
    // It also looks like switching PPS to a different value takes more time than expected
    if (isSoftwarePPS())
    {
        getSerial().sendBlocking(PPSNMEA);
        popGPSMessages("$PMTK001,255");
        getSerial().sendBlocking(HZ2_1000);
        popGPSMessages("$PMTK001,220");
    }
    else
    {
        getSerial().sendBlocking(HZ2_500);
        popGPSMessages("$PMTK001,220");
    }

    getSerial().sendBlocking(CONSTELLATIONS);
    popGPSMessages("$PMTK001,353");

    getSerial().sendBlocking(SBAS);
    popGPSMessages("$PMTK001,313");

    getSerial().sendBlocking(SBASDGPS);
    popGPSMessages("$PMTK001,301");

    getSerial().sendBlocking(PPS3DFIX);
    popGPSMessages("$PMTK001,285");

    getSerial().sendBlocking(ENABLEAIC);
    popGPSMessages("$PMTK001,286");

    getSerial().sendBlocking(STATICNAVTHD);
    popGPSMessages("$PMTK001,386");

    // getSerial().sendBlocking(HOTRESTART);
    // popGPSMessages();

    getSerial().sendBlocking(NOPOWERSAVE);
    popGPSMessages("$PMTK001,225");

    getSerial().sendBlocking(VERSIONNO);
    popGPSMessages("$PQVERNO");

    setStatus("Configured");
    return true;
}