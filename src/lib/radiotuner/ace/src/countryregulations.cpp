
#include "../countryregulations.hpp"
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

uint32_t CountryRegulations::determineFrequency(const CountryRegulations::ProtocolTimeSlot &protocolTimeSlot)
{
    auto const &frequency = protocolTimeSlot.frequency;
    switch (protocolTimeSlot.channelMethod)
    {
    case CountryRegulations::ChannelMethod::CHANNEL_0:
        return frequency.baseFrequency;
    case CountryRegulations::ChannelMethod::CHANNEL_1:
        return frequency.baseFrequency + frequency.channelSeperation;
    default:
        return 0;
    }
}

// New simplified methods
uint8_t CountryRegulations::getFirstSlotIdx(CountryRegulations::Zone zone, GATAS::DataSource dataSource)
{

    for (const auto &slot : timings)
    {
        if (slot.source == dataSource && slot.zone == zone)
        {
            return slot.idx;
        }
    }

    return NONE_DATASOURCE.idx;
}

const CountryRegulations::ProtocolTimeSlot &CountryRegulations::protocolTimeslotById(uint8_t idx)
{
    return timings[idx];
}

uint8_t CountryRegulations::nextSlotIdx(CountryRegulations::Zone zone, uint8_t currentIdx)
{
    const auto &pts = timings[currentIdx];

    // If there was a zone change, get the new protocol handling
    if (pts.zone != zone)
    {
        currentIdx = getFirstSlotIdx(zone, pts.source);
    }

    return timings[currentIdx].nextSlotIdx;
}

CountryRegulations::GetNextTxTimeResult CountryRegulations::getNextTxTime(uint16_t msInSecond, uint8_t currentIdx)
{
    constexpr uint8_t MAXFAILSAFE = 4; // maximum number of tries until give up trying to find a time
    const auto &slot = protocolTimeslotById(currentIdx);

    // Counter just in case that during zone changes no protocol can be found
    uint8_t failSafe = 0; 
    uint16_t randomTime;
    uint8_t idx;

    auto maxRandTime = slot.txMaxTime - slot.txMinTime;
    auto offset = slot.txMinTime + msInSecond;
    do
    {
        randomTime = ((get_rand_64() >> 4) % maxRandTime) + offset;

        // Don't generate a time close to a whole second to prevent
        // a tx time with a rollover seconds. This prevents decryption issues for some protocols mainly FLARM and ogn 3.x.x receiver
        if (slot.source == GATAS::DataSource::FLARM) {
            if ((randomTime % 1000) > 975 && (randomTime % 1000) < 1000)
            {
                randomTime += 25;
            }
        }

        idx = findFittingTimeslot(randomTime, currentIdx);
    }
    while (idx == 0 && failSafe++ <= MAXFAILSAFE);

    // If fail safe, then return a default timeout that always matches
    if (idx == NONE_DATASOURCE.idx)
    {
        idx = findFittingTimeslot(slot.txMinTime + msInSecond, currentIdx);
        return GetNextTxTimeResult{idx, slot.txMinTime};
    }

    return GetNextTxTimeResult{idx, uint16_t(randomTime - msInSecond)};
}

uint8_t CountryRegulations::nextProtocolTimeslot(uint16_t msInSecond, CountryRegulations::Zone zone, GATAS::DataSource source)
{
    msInSecond = msInSecond % 1000;
    auto startIdx = getFirstSlotIdx(zone, source);
    auto currentIdx = getFirstSlotIdx(zone, source);
    do
    {
        const auto &slot = protocolTimeslotById(currentIdx);
        if (msInSecond < slot.slotStartTime)
        {
            return slot.idx;
        }
        currentIdx = slot.nextSlotIdx;
    }
    while (currentIdx > startIdx);
    return startIdx;
}

uint8_t CountryRegulations::findFittingTimeslot(uint16_t currentMs, uint8_t currentIdx)
{
    auto startIdx = currentIdx;
    currentMs = currentMs % 1000;
    do
    {
        const auto & slot = protocolTimeslotById(currentIdx);
        auto endTime = slot.slotStartTime + slot.slotDuration;
        if (currentMs >= slot.slotStartTime && currentMs < endTime)
        {
            return slot.idx;
        }
        auto msWithOffset=currentMs+1000;
        if (msWithOffset >= slot.slotStartTime && msWithOffset < endTime)
        {
            return slot.idx;
        }
        currentIdx = slot.nextSlotIdx;
    }
    while (currentIdx != startIdx);

    return NONE_DATASOURCE.idx;
}
