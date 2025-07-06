
#include "countryregulations_v2.hpp"
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
const CountryRegulations::ProtocolTimeSlot &CountryRegulations::getSlot(CountryRegulations::Zone zone, GATAS::DataSource dataSource)
{
    for (size_t i = 0; i < protocolTimimgs.size(); ++i)
    {
        const auto &slot = protocolTimimgs[i];
        if (slot.radioConfig.dataSource == dataSource && slot.zone == zone)
        {
            return protocolTimimgs[i];
        }
    }
    return protocolTimimgs[0]; // Not found
}

uint32_t CountryRegulations::getFrequency(const Frequency &frequency, CountryRegulations::Channel channel)
{
    switch (channel)
    {
    case Channel::CH0:
        return frequency.baseFrequency + (frequency.channelSeperation * 0);
    case Channel::CH1:
        return frequency.baseFrequency + (frequency.channelSeperation * 1);
    default:
        return frequency.baseFrequency; // NOP
    }
}

uint32_t CountryRegulations::nextRandomTime(const CountryRegulations::ProtocolTimeSlot& pts)
{
    constexpr auto  variations = etl::make_array<int16_t>(0, -200, 200);

    constexpr uint32_t MS_IN_SECOND = 1000;

    const auto now = CoreUtils::timeMs32();
    const auto maxRandTime = pts.txMaxTime - pts.txMinTime;
    const auto baseOffset = ((get_rand_64() >> 4) % maxRandTime) + pts.txMinTime; // >>4 is used, becaus there was a bug where the lowest 4 bits where not truly random

    size_t i = 0;
    do {
        int32_t totalOffset = static_cast<int32_t>(baseOffset) + variations[i];

        uint32_t targetMs = (now + totalOffset) % MS_IN_SECOND;
        size_t slotIndex = targetMs / SLOT_MS;

        if (pts.timing[slotIndex] != Channel::NOP &&
            totalOffset >= pts.txMinTime &&
            totalOffset <= pts.txMaxTime)
        {
            return now + totalOffset;
        }

        ++i;
    } while (i < variations.size());

    // fallback: 1s from now
    return now + 1000;
}
