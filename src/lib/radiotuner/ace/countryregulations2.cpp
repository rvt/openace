
#include "countryregulations2.hpp"
#include "ace/coreutils.hpp"

#include "pico/rand.h"

CountryRegulations2::Zone CountryRegulations2::zone(float lat, float lon)
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
const CountryRegulations2::ProtocolTimeSlot2 &CountryRegulations2::getSlot(CountryRegulations2::Zone zone, GATAS::DataSource dataSource)
{
    for (size_t i = 0; i < timings2.size(); ++i)
    {
        const auto &slot = timings2[i];
        if (slot.radioConfig.dataSource == dataSource && slot.zone == zone)
        {
            return timings2[i];
        }
    }
    return timings2[0]; // Not found
}

uint32_t CountryRegulations2::getFrequency(const Frequency &frequency, CountryRegulations2::Channel channel)
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
