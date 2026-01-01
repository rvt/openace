#pragma once

#include <stdint.h>

#include <etl/array.h>

#include "ace/basemodule.hpp"

class CountryRegulations {
public:
  static constexpr uint32_t SLOT_MS = 200;

  enum Zone : uint8_t {
    ZONE0, // Zone is unknown and no transmission will take place
    ZONE1, // Zone 1: Europe, Africa, Russia, China (30W to 110E, excl. zone 5)
           // All protocols MUST be part of ZONE1. see :
           // RadioTuner::addDataSourceToTasks()
    ZONE2, // Zone 2: North America (west of 30W, north of 10N)
    ZONE3, // Zone 3: New Zealand (east of 160E)
    ZONE4, // Zone 4: Australia (110E to 160E)
    ZONE5, // Zone 5: Israel (34E to 54E and 29.25N to 33.5N
    ZONE6  // Zone 6: South America (west of 30W, south of 10N)
  };

  inline static constexpr GATAS::Mapping<Zone, const char *> ZoneMapping[] = {
      {Zone::ZONE0, "ZONE0"}, {Zone::ZONE1, "ZONE1"}, {Zone::ZONE2, "ZONE2"},
      {Zone::ZONE3, "ZONE3"}, {Zone::ZONE4, "ZONE4"}, {Zone::ZONE5, "ZONE5"},
      {Zone::ZONE6, "ZONE6"},
  };

  static const char *zoneToString(Zone zone) {
    return GATAS::enumToString(ZoneMapping, zone, "UNKNOWN");
  }

  struct Frequency {
    uint32_t baseFrequency;
    uint32_t channelSeperation;
    int8_t powerdBm;
    uint16_t bandwidth;
  };

  enum class Channel : uint8_t {
    // Europe has 2 channels channel 0 = 868.2MHz, channel 1 = 868.4MHz.
    // Frequency is finaly decided by the frequency table per area
    NOOP,
    CH00,
    CH01,
    CH24,
    CH65
  };

  // clang-format off
    // From https://github.com/pjalocha/esp32-ogn-tracker/src/freqplan.h
    static constexpr Frequency Europe       {868'200'000, 200'000, 14, 250};
    static constexpr Frequency NorthAmerica {902'200'000, 400'000, 30, 250};
    static constexpr Frequency NewZealand   {869'250'000, 200'000, 10, 250};
    static constexpr Frequency Australia    {917'000'000, 400'000, 30, 250};
    static constexpr Frequency Israel       {916'200'000, 200'000, 22, 250};
    static constexpr Frequency SouthAmerica {917'000'000, 400'000, 30, 250};

    // First byte of the syncWord is the preamble for TX
    //                                                       mode                     dataSource                packetLength    
    //                                                                                                               txPreambleLength
    //                                                                                                                   syncLength
    //                                                                                                                      syncSkipInRxLength
    //                                                                                                                          SYNC
    static constexpr Radio::ProtocolConfig PROTOCOL_NONE {1, GATAS::Modulation::NONE, GATAS::DataSource::ADSL,    0, 16, 8, 0, {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}};       // NONE
    static constexpr Radio::ProtocolConfig PROTOCOL_FLARM{2, GATAS::Modulation::GFSK, GATAS::DataSource::FLARM,  26, 16, 8, 2, {0x55, 0x99, 0xA5, 0xA9, 0x55, 0x66, 0x65, 0x96}};       // 0 FLARM 0 airtime 6ms    
    static constexpr Radio::ProtocolConfig PROTOCOL_OGN1 {3, GATAS::Modulation::GFSK, GATAS::DataSource::OGN1,   26, 16, 8, 2, {0xAA, 0x66, 0x55, 0xA5, 0x96, 0x99, 0x96, 0x5A}};       // 1 OGN 1 airtime 6ms <- This seems to be in use 20 Byte packet length :: 6 byte CRC
    static constexpr Radio::ProtocolConfig PROTOCOL_ADSL {4, GATAS::Modulation::GFSK, GATAS::DataSource::ADSL,   24, 16, 8, 2, {0x55, 0x99, 0x95, 0xA6, 0x9A, 0x65, 0xA9, 0x6A}};       // 3 ADSL == SYNC  0x72 0x4B = Manchester 0x95, 0xA6, 0x9A, 0x65. 0x99 is required as a preamble to be send and rge length is included as sync because it's fixed to 0x18
    static constexpr Radio::ProtocolConfig PROTOCOL_PAW  {5, GATAS::Modulation::GFSK, GATAS::DataSource::PAW,    00, 16, 8, 0, {0xB4, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x18, 0x71}};       // 4 PAW
    static constexpr Radio::ProtocolConfig PROTOCOL_FANET{6, GATAS::Modulation::LORA, GATAS::DataSource::FANET, 255, 12, 2, 0, {0xF4, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};       // 5 FANET

    struct ProtocolTimeSlot
    {
        uint8_t ptsId;              // Unique ID for each ProtocolTiming. Use internally to oprmise configurrion of the transceiver
        CountryRegulations::Zone zone;
        Frequency frequency;
        Radio::ProtocolConfig radioConfig;
        etl::array<Channel, 5> timing;
        uint16_t txMinTime;
        uint16_t txMaxTime;
        uint8_t waitAfterCatStart;
        uint8_t waitAfterCatEnd;
    };

    //               __in_flash()
    static constexpr auto protocolTimimgs = etl::make_array<const ProtocolTimeSlot>(
        ProtocolTimeSlot{ 1, CountryRegulations::Zone::ZONE0, Europe,       PROTOCOL_NONE,  {Channel::NOOP, Channel::NOOP, Channel::NOOP, Channel::NOOP, Channel::NOOP},  000, 0000, 00, 000},
        ProtocolTimeSlot{ 2, CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_FLARM, {Channel::CH01, Channel::NOOP, Channel::CH00, Channel::CH00, Channel::CH01},  600, 1400, 15, 150},
        ProtocolTimeSlot{ 3, CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_OGN1,  {Channel::CH00, Channel::NOOP, Channel::CH01, Channel::CH01, Channel::CH00},  600, 1400, 15, 150},
        ProtocolTimeSlot{ 4, CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_ADSL,  {Channel::CH00, Channel::NOOP, Channel::CH01, Channel::CH01, Channel::CH00},  600, 1400, 15, 250},
        ProtocolTimeSlot{ 5, CountryRegulations::Zone::ZONE1, Europe,       PROTOCOL_FANET, {Channel::CH00, Channel::CH00, Channel::CH00, Channel::CH00, Channel::CH00}, 2000, 3000, 15, 000},

        ProtocolTimeSlot{ 6, CountryRegulations::Zone::ZONE2, NorthAmerica, PROTOCOL_FLARM, {Channel::CH65, Channel::CH65, Channel::CH65, Channel::CH65, Channel::CH65}, 600, 1400, 15, 150},
        ProtocolTimeSlot{ 7, CountryRegulations::Zone::ZONE2, NorthAmerica, PROTOCOL_OGN1, {Channel::CH65, Channel::CH65, Channel::CH65, Channel::CH65, Channel::CH65}, 600, 1400, 15, 150},
        ProtocolTimeSlot{ 8, CountryRegulations::Zone::ZONE2, NorthAmerica, PROTOCOL_ADSL, {Channel::CH65, Channel::CH65, Channel::CH65, Channel::CH65, Channel::CH65}, 600, 1400, 15, 150},
        ProtocolTimeSlot{ 9, CountryRegulations::Zone::ZONE2, NorthAmerica, PROTOCOL_FANET, {Channel::CH65, Channel::CH65, Channel::CH65, Channel::CH65, Channel::CH65}, 600, 1400, 15, 150},

        ProtocolTimeSlot{10, CountryRegulations::Zone::ZONE5, Israel,       PROTOCOL_FLARM, {Channel::CH24, Channel::CH24, Channel::CH24, Channel::CH24, Channel::CH24}, 600, 1400, 15, 150},

        ProtocolTimeSlot{11, CountryRegulations::Zone::ZONE3, NewZealand,   PROTOCOL_FLARM, {Channel::CH65, Channel::CH65, Channel::CH65, Channel::CH65, Channel::CH65}, 600, 1400, 15, 150},
        ProtocolTimeSlot{12, CountryRegulations::Zone::ZONE3, NewZealand,   PROTOCOL_OGN1,  {Channel::CH65, Channel::CH65, Channel::CH65, Channel::CH65, Channel::CH65}, 600, 1400, 15, 150}
    );

  // clang-format on
private:
  CountryRegulations() = delete;

public:
  /**
   * Based on current lat/long get the current regulation zone
   */
  static Zone zone(float lat, float lon);

  /**
   * @brief get the correct slot for the datasource and Zone. This decides
   * everything on what frequency to use, timings etc..
   *
   * @param zone
   * @param dataSource
   * @return const CountryRegulations::ProtocolTimeSlot& Reference to the slot
   * configuration
   */
  static const CountryRegulations::ProtocolTimeSlot &
  getSlot(CountryRegulations::Zone zone, GATAS::DataSource dataSource);

  /**
   * @brief Decide on the frequency from the configuration and the channel
   * Note: Only CH00 and CH01 is supported, NOOP is not a valid channel
   *
   * @param frequency
   * @param channel
   * @return uint32_t frequency in Hz
   */
  static uint32_t getFrequency(const Frequency &frequency,
                               CountryRegulations::Channel channel);

  /**
   * @brief Create a next random time for this protocol that falls within the
   * timeslot
   *
   * @param pts
   * @return uint32_t
   */
  static uint32_t
  nextRandomTime(const CountryRegulations::ProtocolTimeSlot &pts);

  /**
   * @brief Calculate the frequency channel based on a timestamp.
   *
   * @param timestamp
   * @param nch
   * @return uint32_t
   */
  static uint32_t frequencyByTimestamp(uint32_t timestamp, uint32_t nch);

  /**
   * @brief Get the Frequency based on datasource and lat/lon location. Returns
   * 0 if no frequency could be decided
   *
   * @param lat
   * @param lon
   * @param dataSource
   * @return uint32_t
   */
  static uint32_t getFrequency(float lat, float lon,
                               GATAS::DataSource dataSource);
};
