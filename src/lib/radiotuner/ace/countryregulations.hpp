#pragma once

#include <stdint.h>
#include <stdint.h>

#include <etl/array.h>

#include "ace/basemodule.hpp"

class CountryRegulations
{
public:
    enum Zone : uint8_t
    {
        ZONE0, // ZONE0 is unknown and no transmission will take place
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
    static constexpr Radio::ProtocolConfig PROTOCOL_NONE{1, GATAS::Modulation::GFSK, GATAS::DataSource::NONE,   0,           16,              0,         1, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // NONE
    static constexpr Radio::ProtocolConfig PROTOCOL_FLARM{2, GATAS::Modulation::GFSK, GATAS::DataSource::FLARM, 26,          16,              0,         7, {/*0x55,*/ 0x99, 0xA5, 0xA9, 0x55, 0x66, 0x65, 0x96, 0x00}}; // 0 FLARM 0 airtime 6ms
    static constexpr Radio::ProtocolConfig PROTOCOL_OGN1{3, GATAS::Modulation::GFSK, GATAS::DataSource::OGN1,   26,          16,              0,         8, {0xAA, 0x66, 0x55, 0xA5, 0x96, 0x99, 0x96, 0x5A}}; // 1 OGN 1 airtime 6ms <- This seems to be in use 20 Byte packet length :: 6 byte CRC
    static constexpr Radio::ProtocolConfig PROTOCOL_ADSL{4, GATAS::Modulation::GFSK, GATAS::DataSource::ADSL,   24,          16,              0,         8, {0x55, 0x99, 0x95, 0xA6, 0x9A, 0x65, 0xA9, 0x6A}}; // 3 ADSL == SYNC  0x72 0x4B = Manchester 0x95, 0xA6, 0x9A, 0x65. 0x99 is required as a preamble to be send and rge length is included as sync because it's fixed to 0x18
    static constexpr Radio::ProtocolConfig PROTOCOL_PAW{5, GATAS::Modulation::GFSK, GATAS::DataSource::PAW,     00,          16,              0,         8, {0xB4, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x18, 0x71}}; // 4 PAW
    static constexpr Radio::ProtocolConfig PROTOCOL_FANET{6, GATAS::Modulation::LORA, GATAS::DataSource::FANET, 0xff,        12,              8,         2, {0xF4, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 5 FANET 3

    enum class ChannelMethod : uint8_t
    {
        // Europe has 2 channels channel 0 = 868.2MHz, channel 1 = 868.4MHz. Frequency is finaly decided by the frequency table per area
        CHANNEL_0,
        CHANNEL_1
    };

    struct ProtocolTimeSlot
    {
        uint8_t idx;                              // This slot ID
        uint8_t nextSlotIdx;                      // Next slot to use, this is used to optmise searching for the next ID, specially when it's wrapped back
        CountryRegulations::Zone zone;            // Zone where this regulation applies
        GATAS::DataSource source;               // System Source
        const Frequency &frequency;               // Entry on the regulation table
        const Radio::ProtocolConfig &radioConfig; // Entry on the regulation table
        uint16_t slotStartTime;                   // in ms start time of a slot must be between 0..1000
        uint16_t slotDuration;                    // in ms duration of this slot must be between 0..1000
        uint16_t txMinTime;                       // Minimum time between transmissions on a single frequency
        uint16_t txMaxTime;                       // Maximum time between transmissions on a single frequency
        uint8_t waitAfterCatStart;                // If CAD was detected, minimum time to wait before transmitting
        uint8_t waitAfterCatEnd;                  // If CAD was detected, maximum time to wait before transmitting
        ChannelMethod channelMethod;              // What method channel selection
    };

    static constexpr ProtocolTimeSlot NONE_DATASOURCE = ProtocolTimeSlot{0, 0, CountryRegulations::Zone::ZONE0, GATAS::DataSource::NONE, Europe, PROTOCOL_NONE, 000, 0000, 000, 0000, 00, 000, ChannelMethod::CHANNEL_0};

    // Table with timings for each protocol needs to adhere to the following rules for optimalisation reasons
    // - Minimum of 1 and a maximum of 2 timings packages
    // - slotStartTime of the first timeSlot must become before the slotStartTime of the second timeSlot
    // - slotStartTime must be between 0..1000
    // - Timeslots may not overlap
    // - Timeslots may have one gap (see FLARM has a gap of 200ms between 200 and 400ms in the second)
    // - nextSlotId must contain the index of 'the other' timeslot

    static constexpr __in_flash() etl::array<const ProtocolTimeSlot, 10> timings{
        NONE_DATASOURCE,

        // FLARM packages are send/rceived 400..1200ms after PPS channel is based on FLARM_TIME_BASED_2SLOTS. Minimum 600ms between packages, maximum of 1400ms between packages
        ProtocolTimeSlot{1, 2, CountryRegulations::Zone::ZONE1, GATAS::DataSource::FLARM, Europe, PROTOCOL_FLARM, 400, 400, 600, 1400, 15, 150, ChannelMethod::CHANNEL_0},
        ProtocolTimeSlot{2, 1, CountryRegulations::Zone::ZONE1, GATAS::DataSource::FLARM, Europe, PROTOCOL_FLARM, 800, 400, 600, 1400, 15, 150, ChannelMethod::CHANNEL_1},

        // FLARM Zone 5
        ProtocolTimeSlot{3, 4, CountryRegulations::Zone::ZONE5, GATAS::DataSource::FLARM, Israel, PROTOCOL_FLARM, 400, 400, 600, 1400, 15, 150, ChannelMethod::CHANNEL_0},
        ProtocolTimeSlot{4, 3, CountryRegulations::Zone::ZONE5, GATAS::DataSource::FLARM, Israel, PROTOCOL_FLARM, 800, 400, 600, 1400, 15, 150, ChannelMethod::CHANNEL_1},

        // OGN 
        ProtocolTimeSlot{5, 6, CountryRegulations::Zone::ZONE1, GATAS::DataSource::OGN1, Europe, PROTOCOL_OGN1, 400, 400, 600, 1400, 15, 150, ChannelMethod::CHANNEL_1},
        ProtocolTimeSlot{6, 5, CountryRegulations::Zone::ZONE1, GATAS::DataSource::OGN1, Europe, PROTOCOL_OGN1, 800, 400, 600, 1400, 15, 150, ChannelMethod::CHANNEL_0},

        // ADSL
        ProtocolTimeSlot{7, 8, CountryRegulations::Zone::ZONE1, GATAS::DataSource::ADSL, Europe, PROTOCOL_ADSL, 400, 400, 600, 1400, 15, 250, ChannelMethod::CHANNEL_1},
        ProtocolTimeSlot{8, 7, CountryRegulations::Zone::ZONE1, GATAS::DataSource::ADSL, Europe, PROTOCOL_ADSL, 800, 400, 600, 1400, 15, 250, ChannelMethod::CHANNEL_0},

        // Fanet
        ProtocolTimeSlot{9, 9, CountryRegulations::Zone::ZONE1, GATAS::DataSource::FANET, Europe, PROTOCOL_FANET, 000, 1000, 2000, 3000, 15, 000, ChannelMethod::CHANNEL_0},
    };

private:
    CountryRegulations() = delete;
public:

    /**
     * Based on current lat/long get the current regulation zone
     */
    static Zone zone(float lat, float lon);

    /**
     * Get the next time to transmit a package based on current protocolTimeSlot and lastTxTimeMs
     * Like the other methods it supports 1 or 2 slots only
     * \returns UINT32_MAX if no transmission is allowed otherwhise a value between 0..1000 of when to send a position packet
     */
    static uint16_t getNextTxTime(const CountryRegulations::ProtocolTimeSlot &protocolTimeSlot);

    /**
     * Decide what frequency to use based on time in seconds, current regulation and protocolTimeSlot
     * \returns the freuwqncy in Hz of the current protocol and the current time
     */
    static uint32_t determineFrequency(const CountryRegulations::ProtocolTimeSlot &protocolTimeSlot);

    // New Interface
    /**
     * return the first configuration for the given datasource and zone
     */
    static uint8_t getFirstSlotIdx(CountryRegulations::Zone zone, GATAS::DataSource dataSource);

    /**
     * Return the slot's configuration for the given idx
     */
    static const CountryRegulations::ProtocolTimeSlot &protocolTimeslotById(uint8_t idx);

    /**
     * Returns the idx for the following slot from the given idx
     */
    static uint8_t nextSlotIdx(CountryRegulations::Zone zone, uint8_t currentIdx);

    /**
     * Create a random time till the next transmission given the the rules for the protocol
     */
    struct GetNextTxTimeResult
    {
        uint8_t idx;
        uint16_t duration;
    };
    static GetNextTxTimeResult getNextTxTime(uint16_t msInSecond, uint8_t currentIdx);

    /**
     * Based on the current time in Ms, decide for the given zone and protocol what the next timeslot is going to be
     */
    static uint8_t nextProtocolTimeslot(uint16_t msInSecond, CountryRegulations::Zone zone, GATAS::DataSource dataSource);

    /**
     * Find a timeslot that fits the givenMs
     */
    static uint8_t findFittingTimeslot(uint16_t currentMs, uint8_t currentIdx);
};
