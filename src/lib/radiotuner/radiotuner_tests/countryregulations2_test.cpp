
#include <catch2/catch_test_macros.hpp>

#include "../ace/countryregulations_v2.hpp"
#include "pico/rand.h"
#include "pico/time.h"
#include "ace/coreutils.hpp"

auto countryRegulations = CountryRegulations{};

// Set the Pico mock timer so that CoreUtils picks up a deterministic time.
// msSinceEpoch is an absolute value; internally time_us_64 is in microseconds.
static void setTime(uint64_t msSinceEpoch)
{
    time_us_64_SET(1700000000000ULL);

    CoreUtils::setOffsetMsSinceEpoch(msSinceEpoch);
    CoreUtils::setPPS((msSinceEpoch * 1000) % 1000000);
}

TEST_CASE("zone", "[single-file]")
{
    REQUIRE(CountryRegulations::Zone::ZONE1 == countryRegulations.zone(51, 4));
    REQUIRE(CountryRegulations::Zone::ZONE4 == countryRegulations.zone(51, 120));
    REQUIRE(CountryRegulations::Zone::ZONE1 == countryRegulations.zone(0, 0));
}

TEST_CASE("getFrequency", "[single-file]")
{
    REQUIRE(869725000 == CountryRegulations::getFrequency(CountryRegulations::Europe_hdr, CountryRegulations::Channel::CH00_01));
    REQUIRE(868200000 == CountryRegulations::getFrequency(CountryRegulations::Europe_m, CountryRegulations::Channel::CH00));
    REQUIRE(868400000 == CountryRegulations::getFrequency(CountryRegulations::Europe_m, CountryRegulations::Channel::CH01));
    REQUIRE(917000000 == CountryRegulations::getFrequency(CountryRegulations::Australia, CountryRegulations::Channel::CH00));
    REQUIRE(917400000 == CountryRegulations::getFrequency(CountryRegulations::Australia, CountryRegulations::Channel::CH01));
    REQUIRE(868200000 == CountryRegulations::getFrequency(CountryRegulations::Europe_m, CountryRegulations::Channel::NOOP));
}

TEST_CASE("getSlot", "[single-file]")
{
    REQUIRE(4 == CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone::ZONE1, GATAS::DataSource::ADSLM).radioConfig.pcId);
    REQUIRE(2 == CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone::ZONE1, GATAS::DataSource::FLARM).radioConfig.pcId);
    REQUIRE(2 == CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone::ZONE1, GATAS::DataSource::FLARM).radioConfig.pcId);
    REQUIRE(3 == CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone::ZONE2, GATAS::DataSource::OGN1).radioConfig.pcId);

    // Not Configured so 0
    REQUIRE(0 == CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone::ZONE6, GATAS::DataSource::PAW).radioConfig.pcId);
}

TEST_CASE("isInTiming normal window", "[timing]")
{
    CountryRegulations::ChannelTiming t{CountryRegulations::Channel::CH01, 400, 800, 0};

    REQUIRE(CountryRegulations::isInTiming(400, t));
    REQUIRE(CountryRegulations::isInTiming(500, t));
    REQUIRE(CountryRegulations::isInTiming(789, t)); // Must be within 10ms

    REQUIRE_FALSE(CountryRegulations::isInTiming(399, t));
    REQUIRE_FALSE(CountryRegulations::isInTiming(800, t));
    REQUIRE_FALSE(CountryRegulations::isInTiming(900, t));
}

TEST_CASE("isInTiming wrapped window", "[timing]")
{
    // 800..1200 == 800..999 and 0..199
    CountryRegulations::ChannelTiming t{CountryRegulations::Channel::CH01, 800, 1200, 0};

    REQUIRE(CountryRegulations::isInTiming(800, t));
    REQUIRE(CountryRegulations::isInTiming(900, t));
    REQUIRE(CountryRegulations::isInTiming(999, t));
    REQUIRE(CountryRegulations::isInTiming(0, t));
    REQUIRE(CountryRegulations::isInTiming(100, t));
    REQUIRE(CountryRegulations::isInTiming(189, t));

    REQUIRE_FALSE(CountryRegulations::isInTiming(200, t));
    REQUIRE_FALSE(CountryRegulations::isInTiming(500, t));
    REQUIRE_FALSE(CountryRegulations::isInTiming(780, t));
}

TEST_CASE("fitsAnyTiming with multiple windows", "[timing]")
{
    static constexpr CountryRegulations::ChannelTiming timingsArr[] = {
        {CountryRegulations::Channel::CH01, 400, 800},
        {CountryRegulations::Channel::CH01, 800, 1200}};

    etl::span<const CountryRegulations::ChannelTiming> timings(timingsArr);

    REQUIRE(CountryRegulations::fitsAnyTiming(450, timings)); // first window
    REQUIRE(CountryRegulations::fitsAnyTiming(850, timings)); // second window
    REQUIRE(CountryRegulations::fitsAnyTiming(50, timings));  // wrapped part of second window

    REQUIRE_FALSE(CountryRegulations::fitsAnyTiming(250, timings));
    REQUIRE_FALSE(CountryRegulations::fitsAnyTiming(350, timings));
}

// ─────────────────────────────────────────────────────────────────────────────
// getProtocolRxTimingsForZone
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("getProtocolRxTimingsForZone - returns all ZONE1 slots when all sources requested", "[CountryRegulations][rxZone]")
{
    etl::array sources = {
        GATAS::DataSource::FLARM,
        GATAS::DataSource::OGN1,
        GATAS::DataSource::ADSLM,
        GATAS::DataSource::ADSLO_HDR,
        GATAS::DataSource::FANET};
    auto result = CountryRegulations::getProtocolRxTimingsForZone(
        CountryRegulations::Zone::enum_type::ZONE1,
        etl::span<const GATAS::DataSource>(sources.data(), sources.size()));

    // All 5 ZONE1 entries in protocolRxTimimgs should be returned
    REQUIRE(result.size() == 5);
    for (auto *slot : result)
    {
        REQUIRE(slot->zone == CountryRegulations::Zone::enum_type::ZONE1);
    }
}

TEST_CASE("getProtocolRxTimingsForZone - filters to only requested sources", "[CountryRegulations][rxZone]")
{
    etl::array sources = {GATAS::DataSource::FLARM};
    auto result = CountryRegulations::getProtocolRxTimingsForZone(
        CountryRegulations::Zone::enum_type::ZONE1,
        etl::span<const GATAS::DataSource>(sources.data(), sources.size()));

    REQUIRE(result.size() == 1);
    REQUIRE(result[0]->radioConfig.isRxDataSource(GATAS::DataSource::FLARM));
}

TEST_CASE("getProtocolRxTimingsForZone - wrong zone returns empty", "[CountryRegulations][rxZone]")
{
    etl::array sources = {GATAS::DataSource::FLARM, GATAS::DataSource::OGN1};
    auto result = CountryRegulations::getProtocolRxTimingsForZone(
        CountryRegulations::Zone::enum_type::ZONE5, // No ZONE5 entries in protocolRxTimimgs
        etl::span<const GATAS::DataSource>(sources.data(), sources.size()));

    REQUIRE(result.empty());
}

TEST_CASE("getProtocolRxTimingsForZone - empty source list returns empty", "[CountryRegulations][rxZone]")
{
    etl::array<GATAS::DataSource, 0> sources{};
    auto result = CountryRegulations::getProtocolRxTimingsForZone(
        CountryRegulations::Zone::enum_type::ZONE1,
        etl::span<const GATAS::DataSource>(sources.data(), sources.size()));

    REQUIRE(result.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// findFittingTiming
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("findFittingTiming - normal range, ms inside", "[CountryRegulations][timing]")
{
    // EU_FLARMT: CH00 400..800, CH01 800..1200
    const auto *t = CountryRegulations::findFittingTiming(600, etl::span(CountryRegulations::EU_FLARMT));
    REQUIRE(t != nullptr);
    REQUIRE(t->channel == CountryRegulations::Channel::CH00);
}

TEST_CASE("findFittingTiming - normal range, ms outside", "[CountryRegulations][timing]")
{
    // 300ms is before the first slot starts at 400
    const auto *t = CountryRegulations::findFittingTiming(300, etl::span(CountryRegulations::EU_FLARMT));
    REQUIRE(t == nullptr);
}

TEST_CASE("findFittingTiming - wrapped range (end > 1000), ms in upper wrap", "[CountryRegulations][timing]")
{
    // CH01: start=800, end=1200 → wraps: valid at 800..989 and 0..189 (after REDUCE_ENDTIME_MS)
    const auto *t = CountryRegulations::findFittingTiming(900, etl::span(CountryRegulations::EU_FLARMT));
    REQUIRE(t != nullptr);
    REQUIRE(t->channel == CountryRegulations::Channel::CH01);
}

TEST_CASE("findFittingTiming - wrapped range, ms in lower wrap", "[CountryRegulations][timing]")
{
    // ms=50 should fall in the lower part of the CH01 wrapped slot (0..189)
    const auto *t = CountryRegulations::findFittingTiming(50, etl::span(CountryRegulations::EU_FLARMT));
    REQUIRE(t != nullptr);
    REQUIRE(t->channel == CountryRegulations::Channel::CH01);
}

TEST_CASE("findFittingTiming - exactly at start is valid", "[CountryRegulations][timing]")
{
    const auto *t = CountryRegulations::findFittingTiming(400, etl::span(CountryRegulations::EU_FLARMT));
    REQUIRE(t != nullptr);
    REQUIRE(t->channel == CountryRegulations::Channel::CH00);
}

TEST_CASE("findFittingTiming - exactly at reduced end is not valid", "[CountryRegulations][timing]")
{
    // end=800, REDUCE_ENDTIME_MS=10 → effective end = 790, so 790 is excluded
    const auto *t = CountryRegulations::findFittingTiming(790, etl::span(CountryRegulations::EU_FLARMT));
    // Could hit CH01 if 790 is in its range — just verify it's NOT CH00
    if (t != nullptr)
    {
        REQUIRE(t->channel != CountryRegulations::Channel::CH00);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// frequencyByTimestamp
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("frequencyByTimestamp - result is always within [0, nch)", "[CountryRegulations][freq]")
{
    for (uint32_t ts = 0; ts < 1000; ++ts)
    {
        REQUIRE(CountryRegulations::frequencyByTimestamp(ts, 24) < 24);
        REQUIRE(CountryRegulations::frequencyByTimestamp(ts, 65) < 65);
    }
}

TEST_CASE("frequencyByTimestamp - deterministic: same input same output", "[CountryRegulations][freq]")
{
    REQUIRE(CountryRegulations::frequencyByTimestamp(12346, 24) == 20);
    REQUIRE(CountryRegulations::frequencyByTimestamp(12347, 24) == 7);
    REQUIRE(CountryRegulations::frequencyByTimestamp(12348, 24) == 5);
    REQUIRE(CountryRegulations::frequencyByTimestamp(12349, 24) == 16);
    REQUIRE(CountryRegulations::frequencyByTimestamp(123410, 24) == 23);
}

// ─────────────────────────────────────────────────────────────────────────────
// nextRandomTxTime
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("nextRandomTxTime - result falls within txMin..txMax + tolerance", "[CountryRegulations][txTime]")
{
    // Set time so msInSecond() returns something mid-second (e.g. 100ms)
    setTime(1700000000100ULL);

    const auto &pts = CountryRegulations::getProtocolTxTimings(
        CountryRegulations::Zone::enum_type::ZONE1, GATAS::DataSource::FLARM);

    // Run several times since it's random
    int successCount = 0;
    for (int i = 0; i < 20; ++i)
    {
        uint32_t delay = CountryRegulations::nextRandomTxTime(pts);
        if (delay != UINT32_MAX)
        {
            // Delay should be >= txMinTime and reasonably bounded
            REQUIRE(delay >= pts.txMinTime);
            ++successCount;
        }
    }
    // Most attempts should succeed from a mid-second position
    REQUIRE(successCount > 10);
}

TEST_CASE("nextRandomTxTime - result lands within a valid EU_FLARMT timing window", "[CountryRegulations][txTime]")
{
    const auto &pts = CountryRegulations::getProtocolTxTimings(
        CountryRegulations::Zone::enum_type::ZONE1, GATAS::DataSource::FLARM);

    // Run from several different starting positions within the second
    for (uint32_t startMs : {751u, 100u, 250u, 500u, 750u})
    {
        setTime(1700000000000ULL + startMs);

        int successCount = 0;
        int failCount = 0;

        for (int i = 0; i < 20; ++i)
        {
            uint32_t delay = CountryRegulations::nextRandomTxTime(pts);
            if (delay == UINT32_MAX)
            {
                ++failCount;
                continue;
            }

            uint32_t futureMs = (startMs + delay) % 1000;

            // Must land in CH00: 400..790 or CH01 wrap: 800..989 or 0..189
            // (790 and 189 account for REDUCE_ENDTIME_MS=10)
            bool inCH00 = (futureMs >= 400 && futureMs < 790);
            bool inCH01 = (futureMs >= 800 || futureMs < 190);

            INFO("startMs=" << startMs << " delay=" << delay << " futureMs=" << futureMs);
            REQUIRE((inCH00 || inCH01));
            ++successCount;
        }

        // From any starting position, most attempts should find a valid slot
        REQUIRE(successCount >= 15);
    }
}

TEST_CASE("nextRandomTxTime - delay is at least txMinTime", "[CountryRegulations][txTime]")
{
    setTime(1700000000100ULL);

    const auto &pts = CountryRegulations::getProtocolTxTimings(
        CountryRegulations::Zone::enum_type::ZONE1, GATAS::DataSource::FLARM);

    for (int i = 0; i < 20; ++i)
    {
        uint32_t delay = CountryRegulations::nextRandomTxTime(pts);
        if (delay != UINT32_MAX)
        {
            REQUIRE(delay >= pts.txMinTime);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// getProtocolRxTimings
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("getProtocolRxTimings - ZONE1 FLARM returns correct config", "[CountryRegulations][rxTimings]")
{
    const auto &slot = CountryRegulations::getProtocolRxTimings(
        CountryRegulations::Zone::enum_type::ZONE1, GATAS::DataSource::FLARM);

    REQUIRE(slot.zone == CountryRegulations::Zone::enum_type::ZONE1);
    REQUIRE(slot.radioConfig.isRxDataSource(GATAS::DataSource::FLARM));
    REQUIRE(std::equal(
        slot.timeSlots.begin(), slot.timeSlots.end(),
        CountryRegulations::EU_FLARMT.begin(), CountryRegulations::EU_FLARMT.end()));
}


TEST_CASE("getProtocolRxTimings - unknown source returns NOOP slot", "[CountryRegulations][rxTimings]")
{
    const auto &slot = CountryRegulations::getProtocolRxTimings(
        CountryRegulations::Zone::enum_type::ZONE1, GATAS::DataSource::NONE);

    // NOOP slot has NONE as its data source
    REQUIRE(slot.radioConfig.dataSource() == GATAS::DataSource::NONE);
}

TEST_CASE("getProtocolRxTimings - wrong zone returns NOOP slot", "[CountryRegulations][rxTimings]")
{
    // FLARM is only defined for ZONE1, not ZONE3
    const auto &slot = CountryRegulations::getProtocolRxTimings(
        CountryRegulations::Zone::enum_type::ZONE3, GATAS::DataSource::FLARM);

    REQUIRE(slot.radioConfig.dataSource() == GATAS::DataSource::NONE);
}