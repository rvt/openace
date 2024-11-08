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

    inline static constexpr OpenAce::Mapping<Zone, const char *> ZoneMapping[] =
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
        return OpenAce::enumToString(ZoneMapping, zone, "UNKNOWN");
    }

    struct Frequency
    {
        uint32_t baseFrequency;
        uint32_t channelSeperation;
        uint8_t channels;
        int8_t powerdBm;
    };

    // From https://github.com/VirusPilot/esp32-ogn-tracker
    static constexpr Frequency Europe{868'200'000, 200'000, 02, 14};
    static constexpr Frequency NorthAmerica{902'200'000, 400'000, 65, 30};
    static constexpr Frequency NewZealand{869'250'000, 200'000, 01, 10};
    static constexpr Frequency Australia{917'000'000, 400'000, 24, 30};
    static constexpr Frequency Israel{916'200'000, 200'000, 01, 22};
    static constexpr Frequency SouthAmerica{917'000'000, 400'000, 24, 30};
    static constexpr Frequency EuropeFanet{869'525'000, 000'000, 00, 14};

    // First byte of the syncWord is the preamble for TX
    static constexpr Radio::ProtocolConfig PROTOCOL_NONE{Radio::Mode::GFSK, OpenAce::DataSource::NONE, 0,          1, 1, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // NONE
    static constexpr Radio::ProtocolConfig PROTOCOL_FLARM{Radio::Mode::GFSK, OpenAce::DataSource::FLARM, 24 + 2,   1, 7, {0x55, 0x99, 0xA5, 0xA9, 0x55, 0x66, 0x65, 0x96}}; // 0 FLARM 0 airtime 6ms
    static constexpr Radio::ProtocolConfig PROTOCOL_OGN1{Radio::Mode::GFSK, OpenAce::DataSource::OGN1, 20 + 6,     1, 7, {0xAA, 0x66, 0x55, 0xA5, 0x96, 0x99, 0x96, 0x5A}}; // 1 OGN 1 airtime 6ms <- This seems to be in use 20 Byte packet length :: 6 byte CRC
    static constexpr Radio::ProtocolConfig PROTOCOL_ADSL{Radio::Mode::GFSK, OpenAce::DataSource::ADSL, 24,         2, 6, {0x55, 0x99, 0x95, 0xA6, 0x9A, 0x65, 0xA9, 0x6A}}; // 3 ADSL == SYNC  0x72 0x4B = Manchester 0x95, 0xA6, 0x9A, 0x65. 0x99 is required as a preamble to be send and rge length is included as sync because it's fixed to 0x18
    static constexpr Radio::ProtocolConfig PROTOCOL_PAW{Radio::Mode::GFSK, OpenAce::DataSource::PAW, 00 + 0,       1, 8, {0xB4, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x18, 0x71}}; // 4 PAW
    static constexpr Radio::ProtocolConfig PROTOCOL_FANET{Radio::Mode::LORA, OpenAce::DataSource::FANET, 00 + 0,   1, 8, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 5 FANET 3

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
        OpenAce::DataSource source;               // System Source
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

    //        protocol_t protocol;
    static constexpr ProtocolTimeSlot NONE_DATASOURCE = ProtocolTimeSlot{0, 0, CountryRegulations::Zone::ZONE0, OpenAce::DataSource::NONE, Europe, PROTOCOL_NONE, 000, 0000, 000, 0000, 00, 000, ChannelMethod::CHANNEL_0};

    // Table with timings for each protocol needs to adhere to the following rules for optimalisation reasons
    // - Minimum of 1 and a maximum of 2 timings packages
    // - slotStartTime of the first timeSlot must become before the slotStartTime of the second timeSlot
    // - slotStartTime must be between 0..1000
    // - Timeslots may not overlap
    // - Timeslots may have one gap (see FLARM has a gap of 200ms between 200 and 400ms in the second)
    // - nextSlotId must contain the index of 'the other' timeslot

    static constexpr etl::array<const ProtocolTimeSlot, 10> timings{
        NONE_DATASOURCE,

        // FLARM packages are send/rceived 400..1200ms after PPS channel is based on FLARM_TIME_BASED_2SLOTS. Minimum 600ms between packages, maximum of 1400ms between packages
        ProtocolTimeSlot{1, 2, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::FLARM, Europe, PROTOCOL_FLARM, 400, 400, 600, 1400, 15, 150, ChannelMethod::CHANNEL_0},
        ProtocolTimeSlot{2, 1, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::FLARM, Europe, PROTOCOL_FLARM, 800, 400, 600, 1400, 15, 150, ChannelMethod::CHANNEL_1},

        // FLARM Zone 5
        ProtocolTimeSlot{3, 4, CountryRegulations::Zone::ZONE5, OpenAce::DataSource::FLARM, Israel, PROTOCOL_FLARM, 400, 400, 600, 1400, 15, 150, ChannelMethod::CHANNEL_0},
        ProtocolTimeSlot{4, 3, CountryRegulations::Zone::ZONE5, OpenAce::DataSource::FLARM, Israel, PROTOCOL_FLARM, 800, 400, 600, 1400, 15, 150, ChannelMethod::CHANNEL_1},

        // OGN 
        ProtocolTimeSlot{5, 6, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::OGN1, Europe, PROTOCOL_OGN1, 400, 400, 600, 1400, 15, 150, ChannelMethod::CHANNEL_1},
        ProtocolTimeSlot{6, 5, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::OGN1, Europe, PROTOCOL_OGN1, 800, 400, 600, 1400, 15, 150, ChannelMethod::CHANNEL_0},

        // ADSL
        ProtocolTimeSlot{7, 8, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::ADSL, Europe, PROTOCOL_ADSL, 400, 400, 600, 1400, 15, 250, ChannelMethod::CHANNEL_1},
        ProtocolTimeSlot{8, 7, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::ADSL, Europe, PROTOCOL_ADSL, 800, 400, 600, 1400, 15, 250, ChannelMethod::CHANNEL_0},

        // Fanet
        ProtocolTimeSlot{9, 9, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::FANET, Europe, PROTOCOL_FANET, 000, 1000, 500, 1500, 00, 000, ChannelMethod::CHANNEL_0},
    };

private:
public:
    // Constructor
    CountryRegulations();

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
    static uint8_t getFirstSlotIdx(CountryRegulations::Zone zone, OpenAce::DataSource dataSource);

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
    static uint8_t nextProtocolTimeslot(uint16_t msInSecond, CountryRegulations::Zone zone, OpenAce::DataSource dataSource);

    /**
     * Find a timeslot that fits the givenMs
     */
    static uint8_t findFittingTimeslot(uint16_t currentMs, uint8_t currentIdx);
};
