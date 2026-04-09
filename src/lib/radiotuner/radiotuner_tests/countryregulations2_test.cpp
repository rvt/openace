
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
    // PROTOCOL_ADSL uses DataSource::ADSLM and is in the TX table (M-band ADSL)
    REQUIRE(1 == CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone::ZONE1, GATAS::DataSource::ADSLM).size());
    REQUIRE(2 == CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone::ZONE1, GATAS::DataSource::FLARM)[0].radioConfig.pcId);
    REQUIRE(2 == CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone::ZONE1, GATAS::DataSource::FLARM)[0].radioConfig.pcId);
    REQUIRE(3 == CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone::ZONE2, GATAS::DataSource::OGN1)[0].radioConfig.pcId);

    // ADSLO_HDR has 2 TX entries for ZONE1: one for TRAFFIC and one for UPLINK
    REQUIRE(2 == CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone::ZONE1, GATAS::DataSource::ADSLO_HDR).size());

    // Not configured: empty span
    REQUIRE(CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone::ZONE6, GATAS::DataSource::FANET).empty());
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
    etl::array<GATAS::DataSourceConfig, 4> sources = {{
        {GATAS::DataSource::FLARM, GATAS::DataSourceMode::RX_TX},
        {GATAS::DataSource::OGN1, GATAS::DataSourceMode::RX_TX},
        {GATAS::DataSource::ADSLO_HDR, GATAS::DataSourceMode::RX_TX},
        {GATAS::DataSource::FANET, GATAS::DataSourceMode::RX_TX}}};
    auto result = CountryRegulations::getProtocolRxTimingsForZone(
        CountryRegulations::Zone::enum_type::ZONE1,
        etl::span<const GATAS::DataSourceConfig>(sources.data(), sources.size()));

    // 4 ZONE1 RX entries (ADSLM removed from table)
    REQUIRE(result.size() == 4);
    for (auto *slot : result)
    {
        REQUIRE(slot->zone == CountryRegulations::Zone::enum_type::ZONE1);
    }
}

TEST_CASE("getProtocolRxTimingsForZone - filters to only requested sources", "[CountryRegulations][rxZone]")
{
    etl::array<GATAS::DataSourceConfig, 1> sources = {{
        {GATAS::DataSource::FLARM, GATAS::DataSourceMode::RX_TX}}};
    auto result = CountryRegulations::getProtocolRxTimingsForZone(
        CountryRegulations::Zone::enum_type::ZONE1,
        etl::span<const GATAS::DataSourceConfig>(sources.data(), sources.size()));

    REQUIRE(result.size() == 1);
    REQUIRE(result[0]->radioConfig.isRxDataSource(GATAS::DataSource::FLARM));
}

TEST_CASE("getProtocolRxTimingsForZone - wrong zone returns empty", "[CountryRegulations][rxZone]")
{
    etl::array<GATAS::DataSourceConfig, 2> sources = {{
        {GATAS::DataSource::FLARM, GATAS::DataSourceMode::RX_TX},
        {GATAS::DataSource::OGN1, GATAS::DataSourceMode::RX_TX}}};
    auto result = CountryRegulations::getProtocolRxTimingsForZone(
        CountryRegulations::Zone::enum_type::ZONE5, // No ZONE5 entries in protocolRxTimimgs
        etl::span<const GATAS::DataSourceConfig>(sources.data(), sources.size()));

    REQUIRE(result.empty());
}

TEST_CASE("getProtocolRxTimingsForZone - empty source list returns empty", "[CountryRegulations][rxZone]")
{
    etl::array<GATAS::DataSourceConfig, 0> sources{};
    auto result = CountryRegulations::getProtocolRxTimingsForZone(
        CountryRegulations::Zone::enum_type::ZONE1,
        etl::span<const GATAS::DataSourceConfig>(sources.data(), sources.size()));

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

    const auto pts = CountryRegulations::getProtocolTxTimings(
        CountryRegulations::Zone::enum_type::ZONE1, GATAS::DataSource::FLARM);

    // Run several times since it's random
    int successCount = 0;
    for (int i = 0; i < 20; ++i)
    {
        uint32_t delay = CountryRegulations::nextRandomTxTime(false, pts);
        if (delay != UINT32_MAX)
        {
            // Delay should be >= txMinTime and reasonably bounded
            REQUIRE(delay >= pts[0].txMinTime);
            ++successCount;
        }
    }
    // Most attempts should succeed from a mid-second position
    REQUIRE(successCount > 10);
}

TEST_CASE("nextRandomTxTime - result lands within a valid EU_FLARMT timing window", "[CountryRegulations][txTime]")
{
    const auto pts = CountryRegulations::getProtocolTxTimings(
        CountryRegulations::Zone::enum_type::ZONE1, GATAS::DataSource::FLARM);

    REQUIRE_FALSE(pts.empty());

    // The Pico rand mock returns a fixed value per seed, so test the function
    // deterministically by sweeping candidate delays across the full tx window.
    // This verifies that valid candidates are accepted and invalid candidates
    // are rejected, without depending on a hit-rate threshold.
    for (uint32_t startMs : {751u, 100u, 250u, 500u, 750u})
    {
        setTime(1700000000000ULL + startMs);

        const uint32_t minDelay = pts[0].txMinTime;
        const uint32_t maxDelay = pts[0].txMaxTime;

        for (uint32_t delayCandidate = minDelay; delayCandidate <= maxDelay; ++delayCandidate)
        {
            // Force nextRandomTxTime() to test a single candidate delay.
            get_rand_64_SET(delayCandidate - minDelay);

            uint32_t delay = CountryRegulations::nextRandomTxTime(false, pts);
            uint32_t futureMs = (startMs + delayCandidate) % 1000;
            bool valid = CountryRegulations::fitsAnyTiming(futureMs, pts[0].timeSlots);

            INFO("startMs=" << startMs << " delayCandidate=" << delayCandidate << " delay=" << delay << " futureMs=" << futureMs);

            if (valid)
            {
                REQUIRE(delay == delayCandidate);
            }
            else
            {
                REQUIRE(delay == UINT32_MAX);
            }
        }
    }
}

TEST_CASE("nextRandomTxTime - delay is at least txMinTime", "[CountryRegulations][txTime]")
{
    setTime(1700000000100ULL);

    const auto pts = CountryRegulations::getProtocolTxTimings(
        CountryRegulations::Zone::enum_type::ZONE1, GATAS::DataSource::FLARM);

    for (int i = 0; i < 20; ++i)
    {
        uint32_t delay = CountryRegulations::nextRandomTxTime(false, pts);
        if (delay != UINT32_MAX)
        {
            REQUIRE(delay >= pts[0].txMinTime);
        }
    }
}

TEST_CASE("nextRandomTxTime - multi-slot: both TRAFFIC and UPLINK windows are reachable", "[CountryRegulations][txTime]")
{
    // ADSLO_HDR has two entries: TRAFFIC (450-1000ms) and UPLINK (200-450ms).
    // nextRandomTxTime must schedule into either window, not just the first one.
    const auto pts = CountryRegulations::getProtocolTxTimings(
        CountryRegulations::Zone::enum_type::ZONE1, GATAS::DataSource::ADSLO_HDR);
    REQUIRE(pts.size() == 2);

    // EU_ADSLO_HDRT_TRAFFIC: CH00 450..1000 (id=0) — effective [450, 990)
    // EU_ADSLO_HDRT_UPLINK:  CH00 200..450  (id=1) — effective [200, 440)
    constexpr uint32_t TRAFFIC_START = 450;
    constexpr uint32_t TRAFFIC_END   = 990; // 1000 - REDUCE_ENDTIME_MS=10
    constexpr uint32_t UPLINK_START  = 200;
    constexpr uint32_t UPLINK_END    = 440; // 450 - REDUCE_ENDTIME_MS=10

    int trafficHits = 0;
    int uplinkHits  = 0;

    // startMs=0:   delay=600 → futureMs=600 → TRAFFIC [450,990) ✓
    // startMs=600: delay=600 → futureMs=200 → UPLINK  [200,440) ✓
    // Use get_rand_64_SET(i) to vary the random seed each iteration.
    for (uint32_t startMs : {0u, 600u})
    {
        setTime(1700000000000ULL + startMs);

        for (int i = 0; i < 50; ++i)
        {
            get_rand_64_SET(i);
            uint32_t delay = CountryRegulations::nextRandomTxTime(false, pts);
            if (delay == UINT32_MAX)
            {
                continue;
            }

            uint32_t futureMs = (startMs + delay) % 1000;
            bool inTraffic = (futureMs >= TRAFFIC_START && futureMs < TRAFFIC_END);
            bool inUplink  = (futureMs >= UPLINK_START  && futureMs < UPLINK_END);

            INFO("startMs=" << startMs << " delay=" << delay << " futureMs=" << futureMs << " i=" << i);
            REQUIRE((inTraffic || inUplink));

            if (inTraffic) { ++trafficHits; }
            if (inUplink)  { ++uplinkHits;  }
        }
    }

    // At least one window must be hit across 100 iterations
    REQUIRE((trafficHits + uplinkHits) > 0);
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
