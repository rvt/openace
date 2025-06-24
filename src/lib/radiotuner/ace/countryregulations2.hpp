#pragma once

#include <stdint.h>
#include <stdint.h>

#include <etl/array.h>

#include "ace/basemodule.hpp"

class CountryRegulations2
{
public:
    enum Zone : uint8_t
    {
        ZONE0, // Zone is unknown and no transmission will take place
        ZONE1, // Zone 1: Europe, Africa, Russia, China (30W to 110E, excl. zone 5) All protocols MUST be part of ZONE1. see : RadioTuner::addDataSourceToTasks()
        ZONE2, // Zone 2: North America (west of 30W, north of 10N)
        ZONE3, // Zone 3: New Zealand (east of 160E)
        ZONE4, // Zone 4: Australia (110E to 160E)
        ZONE5, // Zone 5: Israel (34E to 54E and 29.25N to 33.5N
        ZONE6  // Zone 6: South America (west of 30W, south of 10N)
    };

    inline static constexpr GATAS::Mapping<Zone, const char *> ZoneMapping[] =
        {
            {Zone::ZONE0, "ZONE0"},
            {Zone::ZONE1, "ZONE1"},
            {Zone::ZONE2, "ZONE2"},
            {Zone::ZONE3, "ZONE3"},
            {Zone::ZONE4, "ZONE4"},
            {Zone::ZONE5, "ZONE5"},
            {Zone::ZONE6, "ZONE6"},
    };

    static const char *zoneToString(Zone zone)
    {
        return GATAS::enumToString(ZoneMapping, zone, "UNKNOWN");
    }

    struct Frequency
    {
        uint32_t baseFrequency;
        uint32_t channelSeperation;
        uint8_t channels;
        int8_t powerdBm;
        uint16_t bandwidth;
    };

    // From https://github.com/VirusPilot/esp32-ogn-tracker
    static constexpr Frequency Europe{868'200'000, 200'000, 02, 14, 250};
    static constexpr Frequency NorthAmerica{902'200'000, 400'000, 65, 30, 0};
    static constexpr Frequency NewZealand{869'250'000, 200'000, 01, 10, 0};
    static constexpr Frequency Australia{917'000'000, 400'000, 24, 30, 0};
    static constexpr Frequency Israel{916'200'000, 200'000, 01, 22, 0};
    static constexpr Frequency SouthAmerica{917'000'000, 400'000, 24, 30, 0};
    //    static constexpr Frequency PawEurope{869'525'000, 000'000, 00, 14, 100};
    //    static constexpr Frequency EuropeFanet{868'200'000, 000'000, 01, 14, 250};

    // First byte of the syncWord is the preamble for TX
    //                                                                mode                   dataSource  packetLength txPreambleLength codingRate syncLength;
    static constexpr Radio::ProtocolConfig PROTOCOL_NONE{1, GATAS::Modulation::GFSK, GATAS::DataSource::NONE, 0, 16, 0, 8, {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}};              // NONE
    static constexpr Radio::ProtocolConfig PROTOCOL_FLARM{2, GATAS::Modulation::GFSK, GATAS::DataSource::FLARM, 26, 16, 0, 7, {/*0x55,*/ 0x99, 0xA5, 0xA9, 0x55, 0x66, 0x65, 0x96, 0x00}}; // 0 FLARM 0 airtime 6ms
    static constexpr Radio::ProtocolConfig PROTOCOL_OGN1{3, GATAS::Modulation::GFSK, GATAS::DataSource::OGN1, 26, 16, 0, 8, {0xAA, 0x66, 0x55, 0xA5, 0x96, 0x99, 0x96, 0x5A}};             // 1 OGN 1 airtime 6ms <- This seems to be in use 20 Byte packet length :: 6 byte CRC
    static constexpr Radio::ProtocolConfig PROTOCOL_ADSL{4, GATAS::Modulation::GFSK, GATAS::DataSource::ADSL, 24, 16, 0, 8, {0x55, 0x99, 0x95, 0xA6, 0x9A, 0x65, 0xA9, 0x6A}};             // 3 ADSL == SYNC  0x72 0x4B = Manchester 0x95, 0xA6, 0x9A, 0x65. 0x99 is required as a preamble to be send and rge length is included as sync because it's fixed to 0x18
    static constexpr Radio::ProtocolConfig PROTOCOL_PAW{5, GATAS::Modulation::GFSK, GATAS::DataSource::PAW, 00, 16, 0, 8, {0xB4, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x18, 0x71}};               // 4 PAW
    static constexpr Radio::ProtocolConfig PROTOCOL_FANET{6, GATAS::Modulation::LORA, GATAS::DataSource::FANET, 0xff, 12, 8, 2, {0xF4, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};         // 5 FANET 3

    enum class Channel : uint8_t
    {
        // Europe has 2 channels channel 0 = 868.2MHz, channel 1 = 868.4MHz. Frequency is finaly decided by the frequency table per area
        NOP,
        CH0,
        CH1
    };

    struct ProtocolTimeSlot2
    {
        uint8_t ptsId;              // Unique ID for each ProtocolTiming. Use internally to oprmise configurrion of the transceiver
        CountryRegulations2::Zone zone;
        Frequency frequency;
        Radio::ProtocolConfig radioConfig;
        etl::array<Channel, 5> timing;
        uint16_t txMinTime;
        uint16_t txMaxTime;
        uint8_t waitAfterCatStart;
        uint8_t waitAfterCatEnd;
    };

    static constexpr __in_flash() etl::array<const ProtocolTimeSlot2, 9> timings2{
        ProtocolTimeSlot2{1, CountryRegulations2::Zone::ZONE0, Europe, PROTOCOL_NONE, {Channel::NOP, Channel::NOP, Channel::NOP, Channel::NOP, Channel::NOP}, 000, 0000, 00, 000},
        ProtocolTimeSlot2{2, CountryRegulations2::Zone::ZONE1, Europe, PROTOCOL_FLARM, {Channel::CH1, Channel::NOP, Channel::CH0, Channel::CH0, Channel::CH1}, 600, 1400, 15, 150},
        ProtocolTimeSlot2{3, CountryRegulations2::Zone::ZONE1, Europe, PROTOCOL_OGN1, {Channel::CH0, Channel::NOP, Channel::CH1, Channel::CH1, Channel::CH0}, 600, 1400, 15, 150},
        ProtocolTimeSlot2{4, CountryRegulations2::Zone::ZONE1, Europe, PROTOCOL_ADSL, {Channel::CH0, Channel::NOP, Channel::CH1, Channel::CH1, Channel::CH0}, 600, 1400, 15, 250},
        ProtocolTimeSlot2{5, CountryRegulations2::Zone::ZONE1, Europe, PROTOCOL_FANET, {Channel::CH0, Channel::CH0, Channel::CH0, Channel::CH0, Channel::CH0}, 2000, 3000, 15, 000},

        ProtocolTimeSlot2{6, CountryRegulations2::Zone::ZONE5, Israel, PROTOCOL_FLARM, {Channel::CH1, Channel::NOP, Channel::CH0, Channel::CH0, Channel::CH1}, 600, 1400, 15, 150},

        // New Zeland not tested
        ProtocolTimeSlot2{7, CountryRegulations2::Zone::ZONE3, NewZealand, PROTOCOL_FLARM, {Channel::CH1, Channel::NOP, Channel::CH0, Channel::CH0, Channel::CH1}, 600, 1400, 15, 150},
        ProtocolTimeSlot2{8, CountryRegulations2::Zone::ZONE3, NewZealand, PROTOCOL_OGN1, {Channel::CH0, Channel::NOP, Channel::CH1, Channel::CH1, Channel::CH0}, 600, 1400, 15, 150},
        ProtocolTimeSlot2{9, CountryRegulations2::Zone::ZONE1, Europe, PROTOCOL_FANET, {Channel::CH0, Channel::CH0, Channel::CH0, Channel::CH0, Channel::CH0}, 2000, 3000, 15, 000},
    };

private:
    CountryRegulations2() = delete;

public:
    /**
     * Based on current lat/long get the current regulation zone
     */
    static Zone zone(float lat, float lon);

    /**
     * @brief get the correct slot for the datasource and Zone. This decides everything on what frequency to use, timings etc..
     * 
     * @param zone 
     * @param dataSource 
     * @return const CountryRegulations2::ProtocolTimeSlot2& Reference to the slot configuration
     */
    static const CountryRegulations2::ProtocolTimeSlot2 &getSlot(CountryRegulations2::Zone zone, GATAS::DataSource dataSource);

    /**
     * @brief Decide on the frequency from the configuration and the channel
     * Note: Only CH0 and CH1 is supported, NOP is not a valid channel
     * 
     * @param frequency 
     * @param channel 
     * @return uint32_t frequency in Hz
     */
    static uint32_t getFrequency(const Frequency &frequency, CountryRegulations2::Channel channel);
};
