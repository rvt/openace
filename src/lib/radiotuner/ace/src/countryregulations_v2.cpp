
#include "../countryregulations_v2.hpp"
#include "ace/coreutils.hpp"

#include "pico/rand.h"

CountryRegulations::Zone CountryRegulations::zone(float lat, float lon)
{
    if (34.0f <= lon && lon <= 54.0f && 29.25f <= lat && lat <= 33.5f)
    {
        return ZONE5; // Zone 5: Israel (34E to 54E and 29.25N to 33.5N)
    }
    else if (-30.0f <= lon && lon <= 110.0f)
    {
        return ZONE1; // Zone 1: Europe, Africa, Russia, China (30W to 110E, excl. zone 5)
    }
    else if (lon < -30.0f && 10.0f < lat)
    {
        return ZONE2; // Zone 2: North America (west of 30W, north of 10N)
    }
    else if (160.0f < lon)
    {
        return ZONE3; // Zone 3: New Zealand (east of 160E)
    }
    else if (110.0f <= lon && lon <= 160.0f)
    {
        return ZONE4; // Zone 4: Australia (110E to 160E)
    }
    else if (lon < -30.0f && lat < 10.0f)
    {
        return ZONE6; // Zone 6: South America (west of 30W, south of 10N)
    }
    return ZONE0; // not defined
}

// New simplified methods
const CountryRegulations::ProtocolTxTimeSlot &CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone zone, GATAS::DataSource dataSource)
{
    for (size_t i = 0; i < protocolTxTimimgs.size(); ++i)
    {
        const auto &slot = protocolTxTimimgs[i];
        if (slot.radioConfig.isTxDataSource(dataSource) && slot.zone == zone)
        {
            return protocolTxTimimgs[i];
        }
    }
    return CountryRegulations::NOOP_TX_TIMESLOT; // Not found
}

const CountryRegulations::ProtocolRxTimeSlot &CountryRegulations::getProtocolRxTimings(CountryRegulations::Zone zone, GATAS::DataSource dataSource)
{
    for (size_t i = 0; i < protocolRxTimimgs.size(); ++i)
    {
        const auto &slot = protocolRxTimimgs[i];
        if (slot.radioConfig.isRxDataSource(dataSource) && slot.zone == zone)
        {
            return protocolRxTimimgs[i];
        }
    }
    return CountryRegulations::NOOP_TX_TIMESLOT; // Not found
}

uint32_t CountryRegulations::getFrequency(const Frequency &frequency, CountryRegulations::Channel channel)
{
    switch (channel)
    {
    case Channel::CH00:
        return frequency.baseFrequency;
    case Channel::CH01:
        return frequency.baseFrequency + frequency.channelSeperation;
    case Channel::CH00_01:
    {
        if (get_rand_64() & 0x01)
        {
            return frequency.baseFrequency;
        }
        else
        {
            return frequency.baseFrequency + frequency.channelSeperation;
        }
    }
    case Channel::CH24:
        return frequency.baseFrequency + CountryRegulations::frequencyByTimestamp(CoreUtils::secondsSinceEpoch(), 24) * frequency.channelSeperation;
    case Channel::CH65:
        return frequency.baseFrequency + CountryRegulations::frequencyByTimestamp(CoreUtils::secondsSinceEpoch(), 65) * frequency.channelSeperation;
    default:
        return frequency.baseFrequency; // NOP
    }
}

uint32_t CountryRegulations::frequencyByTimestamp(uint32_t timestamp, uint32_t nch)
{
    uint32_t nts = ~timestamp;
    uint32_t ts16 = timestamp * 32768 + nts;
    uint32_t v4096 = (ts16 >> 12) ^ ts16;
    uint32_t v5 = 5 * v4096;
    uint32_t v16 = (v5 >> 4) ^ v5;
    uint32_t v2057 = 2057 * v16;
    uint32_t v9 = (v2057 >> 16) ^ v2057;
    return v9 % nch;
}

uint32_t CountryRegulations::nextRandomTxTime(const CountryRegulations::ProtocolTxTimeSlot &pts)
{
    const uint32_t nowInSecond = CoreUtils::msInSecond();

    const uint32_t minDelay = pts.txMinTime;
    const uint32_t maxDelay = pts.txMaxTime;

    constexpr int MAX_TRIES = 5;

    uint32_t randRange = (uint32_t)(get_rand_64() % (maxDelay - minDelay + 1));
    uint32_t delay = minDelay + randRange;

    for (int i = 0; i < MAX_TRIES; ++i)
    {
        uint32_t futureMs = (nowInSecond + delay) % 1000;
        if (fitsAnyTiming(futureMs, pts.timeSlots))
        {
            return delay;
        }
        delay = delay + 225;
    }

    // No suitable slot found this time
    return UINT32_MAX;
}

uint32_t CountryRegulations::getFrequency(float lat, float lon, GATAS::DataSource dataSource)
{
    auto zone = CountryRegulations::zone(lat, lon);
    const auto &timeSlot = CountryRegulations::getProtocolTxTimings(zone, dataSource);

    // WHen no datasource could be found
    if (timeSlot.zone == CountryRegulations::ZONE0)
    {
        return 0;
    }

    auto timing = CountryRegulations::findFittingTiming(CoreUtils::msInSecond(), timeSlot.timeSlots);
    return CountryRegulations::getFrequency(timeSlot.frequency, timing->channel);
}
