#include <stdio.h>

#include "L76B.hpp"

// *INDENT-OFF*
// clang-format off
static auto BAUDRATE_115200 = CoreUtils::createNmeaChecksum("$PMTK251,115200");
static auto WARMSTART = CoreUtils::createNmeaChecksum("$PMTK102");
static auto HZ2 = CoreUtils::createNmeaChecksum("$PMTK220,500");
static auto CONSTELLATIONS = CoreUtils::createNmeaChecksum("$PMTK353,1,1,1,0,0");
static auto SBAS = CoreUtils::createNmeaChecksum("$PMTK313,1");
static auto SBASDGPS = CoreUtils::createNmeaChecksum("$PMTK301,2");
static auto SENTENCES = CoreUtils::createNmeaChecksum("$PMTK314,0,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
static auto PPSALWAYS = CoreUtils::createNmeaChecksum("$PMTK285,4,50");
// 255 – Sync 1PPS and NMEA Messages
//This command enables or disables synchronization between the 1PPS pulse and the NMEA messages. 
// When enabled, the beginning of the NMEA message on the UART is fixed to between 170 and 180ms after the rising edge of the 1PPS pulse. 
// The NMEA message describes the position and time as of the rising edge of the 1PPS pulse.
static auto PPSNMEA = CoreUtils::createNmeaChecksum("$PMTK255,1");

// clang-format on
// *INDENT-ON*

// Note on PPS:
// L76B uses rising pulse to trigger PPS 
// https://www.researchgate.net/figure/PPS-and-NMEA-timing-for-L76-M33_fig11_318528181
bool L76B::configureGnss()
{
    constexpr auto DELAY_BETWEEN_SENTENCES = 100;
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
        vTaskDelay(DELAY_BETWEEN_SENTENCES);
        getSerial().sendBlocking(WARMSTART);
        vTaskDelay(DELAY_BETWEEN_SENTENCES);
        return false;
    }
    
    if (!getSerial().enableTx(baudrate))
    {
        setStatus("Cfg err m8nCfg");
        setStatusBaud(0);
        return false;
    }

    // Save to BBR so we don't have slow startup delays finding the uart
    getSerial().sendBlocking(HZ2);
    vTaskDelay(DELAY_BETWEEN_SENTENCES);
    getSerial().sendBlocking(CONSTELLATIONS);
    vTaskDelay(DELAY_BETWEEN_SENTENCES);
    getSerial().sendBlocking(SBAS);
    vTaskDelay(DELAY_BETWEEN_SENTENCES);
    getSerial().sendBlocking(SBASDGPS);
    vTaskDelay(DELAY_BETWEEN_SENTENCES);
    getSerial().sendBlocking(PPSALWAYS);
    vTaskDelay(DELAY_BETWEEN_SENTENCES);
    getSerial().sendBlocking(PPSNMEA);
    vTaskDelay(DELAY_BETWEEN_SENTENCES);
    getSerial().sendBlocking(WARMSTART);
    vTaskDelay(DELAY_BETWEEN_SENTENCES);

    setStatus("Configured");
    return true;
}