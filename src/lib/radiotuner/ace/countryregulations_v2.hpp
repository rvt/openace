#pragma once

#include <stdint.h>

#include <etl/array.h>
#include <etl/span.h>

#include "ace/basemodule.hpp"

class CountryRegulations
{
public:
    static constexpr uint32_t SLOT_MS = 200;
    static constexpr uint32_t SLOT_IN_S = 1000 / SLOT_MS;

    enum Zone : uint8_t
    {
        ZONE0, // Zone is unknown and no transmission will take place
        ZONE1, // Zone 1: Europe, Africa, Russia, China (30W to 110E, excl. zone 5)
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

    // struct Zone
    // {
    //   enum enum_type : uint8_t
    //   {
    //     ZONE0, // Zone is unknown and no transmission will take place
    //     ZONE1, // Zone 1: Europe, Africa, Russia, China (30W to 110E, excl. zone 5) All protocols MUST be part of ZONE1. see : RadioTuner::addDataSourceToTasks()
    //     ZONE2, // Zone 2: North America (west of 30W, north of 10N)
    //     ZONE3, // Zone 3: New Zealand (east of 160E)
    //     ZONE4, // Zone 4: Australia (110E to 160E)
    //     ZONE5, // Zone 5: Israel (34E to 54E and 29.25N to 33.5N
    //     ZONE6  // Zone 6: South America (west of 30W, south of 10N)
    //   };

    //   ETL_DECLARE_ENUM_TYPE(Zone, uint8_t)
    //   ETL_ENUM_TYPE(ZONE0, "ZONE0")
    //   ETL_ENUM_TYPE(ZONE1, "ZONE1")
    //   ETL_ENUM_TYPE(ZONE2, "ZONE2")
    //   ETL_ENUM_TYPE(ZONE3, "ZONE3")
    //   ETL_ENUM_TYPE(ZONE4, "ZONE4")
    //   ETL_ENUM_TYPE(ZONE5, "ZONE5")
    //   ETL_ENUM_TYPE(ZONE6, "ZONE6")
    //   ETL_END_ENUM_TYPE
    // };

    static const char *zoneToString(Zone zone)
    {
        return GATAS::enumToString(ZoneMapping, zone, "UNKNOWN");
    }

    struct Frequency
    {
        uint32_t baseFrequency;
        uint32_t channelSeperation;
        int8_t powerdBm;
    };

    enum class Channel : uint8_t
    {
        // Europe has 2 channels channel 0 = 868.2MHz, channel 1 = 868.4MHz. Frequency is finaly decided by the frequency table per area
        NOOP,
        CH00,    // Channel 0 base Frequency
        CH01,    // Channel 1 base Frequency + separation
        CH00_01, // Random pick channel 0 or 1
        CH24,    // Channel by epoch based on 24 channels (see frequencyByTimestamp)
        CH65     // Channel by epoch based on 64 channels (see frequencyByTimestamp)
    };

     struct ChannelTiming
    {
        // Remove 20ms from the end time so TX timings will not (eadsely) overflow into a region that won't be received
        static constexpr uint16_t REDUCE_ENDTIME_MS = 10;
        Channel channel;
        uint16_t start;
        uint16_t end;
        uint8_t id;
    };

    struct ProtocolRxTimeSlot
    {
        CountryRegulations::Zone zone;
        Frequency frequency;
        GATAS::ProtocolConfig radioConfig;
        etl::span<const ChannelTiming> timeSlots;
    };

    struct ProtocolTxTimeSlot : public ProtocolRxTimeSlot
    {
        uint16_t txMinTime;
        uint16_t txMaxTime;
        uint8_t waitAfterCatStart;
        uint8_t waitAfterCatEnd;
    };

    // clang-format off
    // From https://github.com/pjalocha/esp32-ogn-tracker/src/freqplan.h
    static constexpr Frequency Europe       {868'200'000, 200'000, 14}; // 868.2 / 868.4
    static constexpr Frequency Europe_o     {869'525'000, 200'000, 22}; // 869.4 .. 869.65
    static constexpr Frequency NorthAmerica {902'200'000, 400'000, 30};
    static constexpr Frequency NewZealand   {869'250'000, 200'000, 10};
    static constexpr Frequency Australia    {917'000'000, 400'000, 30};
    static constexpr Frequency Israel       {916'200'000, 200'000, 22};
    static constexpr Frequency SouthAmerica {917'000'000, 400'000, 30};

    // First byte of the syncWord is the preamble for TX
    //                                                            mode                          dataSource                            packetLength when 0 enables variable packet length mode
    //                                                                                                                                    txPreambleLength
    //                                                                                                                                        syncLength SYNC;
    //                                                                                                                                           skip sync bits in RX mode
    static constexpr GATAS::ProtocolConfig PROTOCOL_NONE          { 1, GATAS::Modulation::NONE, GATAS::DataSource::NONE,       true,  0, 16, 64, 0, {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}};       // NONE
    static constexpr GATAS::ProtocolConfig PROTOCOL_FLARM         { 2, GATAS::Modulation::GFSK, GATAS::DataSource::FLARM,      true, 26, 16, 64, 8, {0x55, 0x99, 0xA5, 0xA9, 0x55, 0x66, 0x65, 0x96}};       // FLARM 0 airtime 6ms
    static constexpr GATAS::ProtocolConfig PROTOCOL_OGN           { 3, GATAS::Modulation::GFSK, GATAS::DataSource::OGN1,       true, 26, 16, 64, 8, {0xAA, 0x66, 0x55, 0xA5, 0x96, 0x99, 0x96, 0x5A}};       // OGN 1 airtime 6ms <- This seems to be in use 20 Byte packet length :: 6 byte CRC

    static constexpr GATAS::ProtocolConfig PROTOCOL_ADSL          { 6, GATAS::Modulation::GFSK, GATAS::DataSource::ADSLM,      true, 25, 16, 48, 6, {0x55, 0x99, 0x95, 0xA6, 0x9A, 0x65}};       // ADSL on normal 0xA9, 0x6A => 0x18
    static constexpr GATAS::ProtocolConfig PROTOCOL_ADSLO_HDR     { 7, GATAS::Modulation::GFSK, GATAS::DataSource::ADSLO_HDR, false,  0, 16, 16, 0, {0x2D, 0xD4}};                                           // ADSL on O band HDR
    static constexpr GATAS::ProtocolConfig PROTOCOL_PAW           { 9, GATAS::Modulation::GFSK, GATAS::DataSource::PAW,        true, 00, 16, 64, 0, {0xB4, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x18, 0x71}};       // Pilot Aware (ogn tracker calls this LDR???)
    static constexpr GATAS::ProtocolConfig PROTOCOL_FANET         {10, GATAS::Modulation::LORA, GATAS::DataSource::FANET,     false, 255, 12, 16, 0, {0xF4, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};       // FANET

    // Needs further research I did not get reliable reception yet
    static constexpr GATAS::ProtocolConfig PROTOCOL_RX_ADSLFLARM  { 5, GATAS::Modulation::GFSK, GATAS::DataSource::ADSLFLARM,  true,  29, 16, 16, 0, {0x96, 0xA5}};     // works to receive FLARM                                     // ADSL/FLARM RX Sync
    static constexpr GATAS::ProtocolConfig PROTOCOL_RX_ADSLOGN    { 4, GATAS::Modulation::GFSK, GATAS::DataSource::ADSLOGN,    true,  29, 16, 16, 0, {0x99, 0x95}};     // ADSL/OGN RX SYNC
    // Needs further research I did not get reliable reception yet

    // Europe
    static constexpr etl::array<ChannelTiming, 1> NOOP {ChannelTiming{Channel::CH00,     00, 1000, 0}};
    static constexpr etl::array EU_FLARMT              {ChannelTiming{Channel::CH00,    400,  800, 0}, ChannelTiming{Channel::CH01, 800, 1200, 0}};
    static constexpr etl::array EU_OGNT                {ChannelTiming{Channel::CH01,    400,  800, 0}, ChannelTiming{Channel::CH00, 800, 1200, 0}};
    static constexpr etl::array EU_ADSLT               {ChannelTiming{Channel::CH00_01, 450, 1000, 0}};
    static constexpr etl::array EU_ADSLO               {ChannelTiming{Channel::CH00_01, 450, 1000, 0}};
    static constexpr etl::array EU_ADSLT_V1            {ChannelTiming{Channel::CH01,    400,  800, 0}, ChannelTiming{Channel::CH00, 800, 1200, 0}};

    static constexpr etl::array EU_ADSLFLARMT          {ChannelTiming{Channel::CH00,    400,  800, 0}, ChannelTiming{Channel::CH01, 800, 1200, 0}};
    static constexpr etl::array EU_ADSLOGNT            {ChannelTiming{Channel::CH01,    400,  800, 0}, ChannelTiming{Channel::CH00, 800, 1200, 0}};
    static constexpr etl::array EU_ADSLO_HDR_RX        {ChannelTiming{Channel::CH00,    200,  450, 0}};

    static constexpr etl::array EU_ADSLO_HDR_TX        {ChannelTiming{Channel::CH00_01, 200,  450, 0}};
    static constexpr etl::array EU_FANETT              {ChannelTiming{Channel::CH00,      0, 1000, 0}};

    // North America
    static constexpr etl::array NA_FLARMT              {ChannelTiming{Channel::CH65, 0, 1000, 0}};
    static constexpr etl::array NA_OGNT                {ChannelTiming{Channel::CH65, 0, 1000, 0}};
    static constexpr etl::array NA_ADSL                {ChannelTiming{Channel::CH65, 0, 1000, 0}};

    static constexpr ProtocolTxTimeSlot NOOP_TX_TIMESLOT{ CountryRegulations::Zone::ZONE0, Europe,       PROTOCOL_NONE,  etl::span(NOOP),  0, 0, 0, 0};
    static constexpr ProtocolRxTimeSlot NOOP_RX_TIMESLOT{ CountryRegulations::Zone::ZONE0, Europe,       PROTOCOL_NONE,  etl::span(NOOP)};

    /**
     * When adding additional zones, all zones must be grpouped together
     */
    static constexpr etl::array protocolRxTimimgs {
        // Needs further research
        // ProtocolRxTimeSlot{ CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_RX_ADSLOGN,    etl::span(EU_ADSLOGNT)},
        // ProtocolRxTimeSlot{ CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_RX_ADSLFLARM,  etl::span(EU_ADSLFLARMT)},
        // Needs further research

        ProtocolRxTimeSlot{ CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_ADSL,     etl::span(EU_ADSLT)},
        ProtocolRxTimeSlot{ CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_OGN,      etl::span(EU_OGNT)},
        ProtocolRxTimeSlot{ CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_FLARM,    etl::span(EU_FLARMT)},
        ProtocolRxTimeSlot{ CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_FANET,    etl::span(EU_FANETT)}
    };

    /**
     * When adding additional zones, all zones must be grpouped together
     */
    static constexpr auto protocolTxTimimgs = etl::make_array<const ProtocolTxTimeSlot>(
        ProtocolTxTimeSlot{ CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_FLARM,         etl::span(EU_FLARMT),       600, 1400, 15, 150},
        ProtocolTxTimeSlot{ CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_OGN,           etl::span(EU_OGNT),         600, 1400, 15, 150},
        ProtocolTxTimeSlot{ CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_ADSL,          etl::span(EU_ADSLT),        600, 1400, 15, 250},
        ProtocolTxTimeSlot{ CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_ADSLO_HDR,     etl::span(EU_ADSLO_HDR_TX), 600, 1400, 15, 250},
        ProtocolTxTimeSlot{ CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_FANET,         etl::span(EU_FANETT),      2000, 3000, 15, 000},

        ProtocolTxTimeSlot{ CountryRegulations::Zone::ZONE2, NorthAmerica, PROTOCOL_FLARM,         etl::span(NA_FLARMT),       600, 1400, 15, 150},
        ProtocolTxTimeSlot{ CountryRegulations::Zone::ZONE2, NorthAmerica, PROTOCOL_OGN,           etl::span(NA_OGNT),         600, 1400, 15, 150},
        ProtocolTxTimeSlot{ CountryRegulations::Zone::ZONE2, NorthAmerica, PROTOCOL_ADSL,          etl::span(NA_ADSL),         600, 1400, 15, 150}
    );
    // clang-format on

    static constexpr uint8_t validateProtocolTxTimings()
    {
        for (auto &pts : protocolTxTimimgs)
        {
            // 2) txMin <= txMax
            if (pts.txMinTime > pts.txMaxTime)
            {
                return 2;
            }

            // 3) timing span must not be empty except PROTOCOL_NONE
            if (pts.radioConfig.dataSource() != GATAS::DataSource::NONE && pts.timeSlots.size() == 0)
            {
                return 3;
            }

            if (pts.zone == CountryRegulations::Zone::ZONE0)
            {
                return 7;
            }

            if (pts.radioConfig.dataSource() >= GATAS::DataSource::_TRANSPROTOCOLS)
            {
                return 4;
            }

            // 4) Validate each ChannelTiming
            for (auto &t : pts.timeSlots)
            {
                if (t.start == t.end)
                {
                    return 5;
                }
                if (t.start >= 2000 || t.end >= 2000)
                {
                    return 6;
                }
            }
        }

        for (auto &pts : protocolTxTimimgs)
        {

            // 2) txMin <= txMax
            if (pts.txMinTime > pts.txMaxTime)
            {
                return 12;
            }

            // 3) timing span must not be empty except PROTOCOL_NONE
            if (pts.radioConfig.dataSource() != GATAS::DataSource::NONE && pts.timeSlots.size() == 0)
            {
                return 13;
            }

            if (pts.zone == CountryRegulations::Zone::ZONE0)
            {
                return 17;
            }

            if (pts.radioConfig.dataSource() >= GATAS::DataSource::_TRANSPROTOCOLS)
            {
                return 14;
            }

            // 4) Validate each ChannelTiming
            for (auto &t : pts.timeSlots)
            {
                if (t.start == t.end)
                {
                    return 15;
                }
                if (t.start >= 2000 || t.end >= 2000)
                {
                    return 16;
                }
            }
        }

        return 0;
    }

private:
    CountryRegulations() = delete;

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
     * @return const CountryRegulations::ProtocolTimeSlot& Reference to the slot configuration
     */
    static const CountryRegulations::ProtocolTxTimeSlot &getProtocolTxTimings(CountryRegulations::Zone zone, GATAS::DataSource dataSource);
    static const CountryRegulations::ProtocolRxTimeSlot &getProtocolRxTimings(CountryRegulations::Zone zone, GATAS::DataSource dataSource);

    /**
     * @brief Decide on the frequency from the configuration and the channel
     * Note: Only CH00 and CH01 is supported, NOOP is not a valid channel
     *
     * @param frequency
     * @param channel
     * @return uint32_t frequency in Hz
     */
    static uint32_t getFrequency(const Frequency &frequency, CountryRegulations::Channel channel);

    /**
     * @brief Create a next random time for this protocol that falls within the timeslot
     *
     * @param pts
     * @return uint32_t
     */
    static uint32_t nextRandomTxTime(const CountryRegulations::ProtocolTxTimeSlot &pts);

    /**
     * @brief Calculate the frequency channel based on a timestamp.
     *
     * @param timestamp
     * @param nch
     * @return uint32_t
     */
    static uint32_t frequencyByTimestamp(uint32_t timestamp, uint32_t nch);

    /**
     * @brief Get the Frequency based on datasource and lat/lon location. Returns 0 if no frequency could be decided
     *
     * @param lat
     * @param lon
     * @param dataSource
     * @return uint32_t
     */
    static uint32_t getFrequency(float lat, float lon, GATAS::DataSource dataSource);

    static bool isInTiming(uint32_t ms, const ChannelTiming &t)
    {
        // ms is in range 0..999
        ms = ms % 1000;
        uint32_t start = t.start % 1000;
        uint32_t end =(t.end-ChannelTiming::REDUCE_ENDTIME_MS) % 1000;

        if (start < end)
        {
            // Normal case: e.g. 400..800
            return (ms >= start) && (ms < end);
        }
        else
        {
            // Wrapped case: e.g. 800..1200 == 800..999 and 0..199
            return (ms >= start) || (ms < end);
        }
    }

    static bool fitsAnyTiming(uint32_t ms, etl::span<const ChannelTiming> timings)
    {
        for (const auto &t : timings)
        {
            if (isInTiming(ms, t))
                return true;
        }
        return false;
    }

    static const ChannelTiming *findFittingTiming(uint32_t ms, etl::span<const ChannelTiming> timings)
    {
        for (const auto &t : timings)
        {
            if (isInTiming(ms, t))
                return &t;
        }
        return nullptr;
    }

    static etl::vector<const ProtocolRxTimeSlot *, GATAS_MAX_SOURCE_PER_RADIO * GATAS_MAX_RADIOS> getProtocolRxTimingsForZone(Zone zone, const etl::span<const GATAS::DataSource> dataSources)
    {
        etl::vector<const ProtocolRxTimeSlot *, GATAS_MAX_SOURCE_PER_RADIO * GATAS_MAX_RADIOS> result;

        for (const auto &slot : protocolRxTimimgs)
        {
            auto isDs = etl::find_if(dataSources.begin(), dataSources.end(), [slot](GATAS::DataSource ds)
                                     { return slot.radioConfig.isRxDataSource(ds); });

            bool addMe = isDs != dataSources.end();

            if (slot.zone == zone && !result.full() && addMe)
            {
                result.push_back(&slot);
            }
        }

        return result;
    }
};
