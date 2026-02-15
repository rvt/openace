
#include <catch2/catch_test_macros.hpp>

#include "../ace/countryregulations_v2.hpp"
#include "pico/rand.h"
#include "pico/time.h"

auto countryRegulations = CountryRegulations{};

TEST_CASE("zone", "[single-file]")
{
    REQUIRE(CountryRegulations::ZONE1 == countryRegulations.zone(51, 4));
    REQUIRE(CountryRegulations::ZONE4 == countryRegulations.zone(51, 120));
    REQUIRE(CountryRegulations::ZONE1 == countryRegulations.zone(0, 0));
}

TEST_CASE("getFrequency", "[single-file]")
{
    REQUIRE(868200000 == CountryRegulations::getFrequency(CountryRegulations::Europe, CountryRegulations::Channel::CH00));
    REQUIRE(868400000 == CountryRegulations::getFrequency(CountryRegulations::Europe, CountryRegulations::Channel::CH01));
    REQUIRE(917000000 == CountryRegulations::getFrequency(CountryRegulations::Australia, CountryRegulations::Channel::CH00));
    REQUIRE(917400000 == CountryRegulations::getFrequency(CountryRegulations::Australia, CountryRegulations::Channel::CH01));
    REQUIRE(868200000 == CountryRegulations::getFrequency(CountryRegulations::Europe, CountryRegulations::Channel::NOOP));
}

// TEST_CASE("getSlot", "[single-file]")
// {
//     REQUIRE(1 == CountryRegulations::getProtocolTxTimings(CountryRegulations::ZONE6, GATAS::DataSource::PAW).ptsId);
//     REQUIRE(4 == CountryRegulations::getProtocolTxTimings(CountryRegulations::ZONE1, GATAS::DataSource::ADSL).ptsId);
//     REQUIRE(2 == CountryRegulations::getProtocolTxTimings(CountryRegulations::ZONE1, GATAS::DataSource::FLARM).ptsId);
// }

TEST_CASE("getFrequency lat/lon/datasource", "[single-file]")
{
    auto frequency =  CountryRegulations::getFrequency(37.420, -121.714, GATAS::DataSource::FLARM);

    REQUIRE(frequency == 910200000);
}

TEST_CASE("isInTiming normal window", "[timing]")
{
    CountryRegulations::ChannelTiming t{CountryRegulations::Channel::CH01, 400, 800, 0};

    REQUIRE(CountryRegulations::isInTiming(400, t));
    REQUIRE(CountryRegulations::isInTiming(500, t));
    REQUIRE(CountryRegulations::isInTiming(789, t)); // Mist be within 10ms

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
        {CountryRegulations::Channel::CH01, 800, 1200}
    };

    etl::span<const CountryRegulations::ChannelTiming> timings(timingsArr);

    REQUIRE(CountryRegulations::fitsAnyTiming(450, timings));  // first window
    REQUIRE(CountryRegulations::fitsAnyTiming(850, timings));  // second window
    REQUIRE(CountryRegulations::fitsAnyTiming(50, timings));   // wrapped part of second window

    REQUIRE_FALSE(CountryRegulations::fitsAnyTiming(250, timings));
    REQUIRE_FALSE(CountryRegulations::fitsAnyTiming(350, timings));
}

// TEST_CASE("nextRandomTime finds valid slot on first try", "[timing]")
// {
//     // Now = 750 ms
//     time_us_64_SET(750000);

//     // Random = 610 -> delay = 600 + (610 % 801) = something in range
//     get_rand_64_SET(325); // deterministic

//     static constexpr CountryRegulations::ChannelTiming timingsArr[] = {
//         {CountryRegulations::Channel::CH01, 400, 800, 0},
//         {CountryRegulations::Channel::CH01, 800, 1200, 0}
//     };

//     CountryRegulations::ProtocolTimeSlot pts{
//         4,
//         CountryRegulations::Zone::ZONE1,
//         CountryRegulations::Europe,
//         CountryRegulations::PROTOCOL_ADSL,
//         etl::span(timingsArr),
//         600,
//         1400,
//         15,
//         250
//     };

//     uint32_t d = CountryRegulations::nextRandomTime(pts);

//     REQUIRE(d >= 600);
//     REQUIRE(d <= 1400);

//     uint32_t future = (750 + d) % 1000;
//     REQUIRE(CountryRegulations::fitsAnyTiming(future, pts.timing));
// }

// TEST_CASE("nextRandomTime retries and succeeds", "[timing]")
// {
//     time_us_64_SET(750000);

//     static constexpr CountryRegulations::ChannelTiming timingsArr[] = {
//         {CountryRegulations::Channel::CH01, 400, 800, 0},
//         {CountryRegulations::Channel::CH01, 800, 1200, 0}
//     };

//     CountryRegulations::ProtocolTimeSlot pts{
//         4,
//         CountryRegulations::Zone::ZONE1,
//         CountryRegulations::Europe,
//         CountryRegulations::PROTOCOL_ADSL,
//         etl::span(timingsArr),
//         600,
//         1400,
//         15,
//         250
//     };

//     // First try: pick something that will NOT fit
//     get_rand_64_SET(10);   // small delay -> likely miss

//     uint32_t d1 = CountryRegulations::nextRandomTime(pts);

//     // Might fail or succeed depending on mapping, so force failure test separately
//     // Better: test guaranteed failure case:
// }

// TEST_CASE("nextRandomTime fails when no slot fits", "[timing]")
// {
//     time_us_64_SET(100000); // now = 100 ms

//     // Timing window: only 900..950
//     static constexpr CountryRegulations::ChannelTiming timingsArr[] = {
//         {CountryRegulations::Channel::CH01, 900, 950, 0}
//     };

//     CountryRegulations::ProtocolTimeSlot pts{
//         1,
//         CountryRegulations::Zone::ZONE1,
//         CountryRegulations::Europe,
//         CountryRegulations::PROTOCOL_ADSL,
//         etl::span(timingsArr),
//         100,   // delays 100..200
//         200,
//         0,
//         0
//     };

//     // No matter what, (100 + delay) % 1000 is 200..300 -> never in 900..950

//     get_rand_64_SET(0);

//     uint32_t d = CountryRegulations::nextRandomTime(pts);

//     REQUIRE(d == UINT32_MAX);
// }

