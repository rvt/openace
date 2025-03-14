
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../fanet/utils.hpp"
#include "../fanet/tracking.hpp"
#include "helpers.hpp"

using namespace FANET;

TEST_CASE("toScaled unsigned 1 2 ", "[Utils]")
{
    auto result = toScaled<uint16_t, etl::ratio<1>, etl::ratio<2>, 7>(25.f);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == 25);

    result = toScaled<uint16_t, etl::ratio<1>, etl::ratio<2>, 7>(50.f);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == 50);

    result = toScaled<uint16_t, etl::ratio<1>, etl::ratio<2>, 7>(64.f);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == 64);

    result = toScaled<uint16_t, etl::ratio<1>, etl::ratio<2>, 7>(255.f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == 127);
}

TEST_CASE("toScaled unsigned 0.5 2.5 ", "[Utils]")
{
    auto result = toScaled<uint16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(25.f);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == 50);

    result = toScaled<uint16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(50.f);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == 100);

    result = toScaled<uint16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(64.f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == 26);

    // outlier
    result = toScaled<uint16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(9999.f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == 127);

    result = toScaled<uint16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(-100);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == 0);
}

TEST_CASE("toScaled signed 0.5 2.5 ", "[Utils]")
{
    auto result = toScaled<int16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(25.f);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == 50);

    result = toScaled<int16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(50.f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == 20);

    result = toScaled<int16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(64.f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == 26);

    // outlier
    result = toScaled<int16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(9999.f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == 63);

    // Negatives
    result = toScaled<int16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(-25.f);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == -50);

    result = toScaled<int16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(-50.f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == -20);

    result = toScaled<int16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(-64.f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == -26);

    // outliers
    result = toScaled<int16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(-9999.f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == -63);
}

// Tests against original code


TEST_CASE("toScaled climbRate", "[Utils]")
{
    auto result = toScaled<int16_t, etl::ratio<1, 10>, etl::ratio<1, 2>, 7>(-2.5f);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == climbRate_Origional(-2.5f));

    result = toScaled<int16_t, etl::ratio<1, 10>, etl::ratio<1, 2>, 7>(-20.5f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == climbRate_Origional(-20.5f));
}


TEST_CASE("toScaled turnRate", "[Utils]")
{
    auto result = toScaled<int16_t, etl::ratio<1, 4>, etl::ratio<1, 1>, 7>(-2.5f);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == turnRate_Origional(-2.5f));

    result = toScaled<int16_t, etl::ratio<1, 4>, etl::ratio<1, 1>, 7>(30.f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == turnRate_Origional(30.f));
}


TEST_CASE("toScaled speed", "[Utils]")
{
    auto result = toScaled<uint16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(40.5f);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == speed_Origional(40.5f));

    result = toScaled<uint16_t, etl::ratio<1, 2>, etl::ratio<5, 2>, 7>(135.5f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == speed_Origional(135.5f));
}


TEST_CASE("toScaled altitude", "[Utils]")
{
    auto result = toScaled<uint16_t, etl::ratio<1, 1>, etl::ratio<4, 1>, 11>(1500.f);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == altitude_Origional(1500.f));

    result = toScaled<uint16_t, etl::ratio<1, 1>, etl::ratio<4, 1>, 11>(5000.f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == altitude_Origional(5000.f));
}

TEST_CASE("Calculate AirTime", "[Utils]")
{
    uint32_t MINUTE = 1000*60;
    AirTime airtime;
    REQUIRE(airtime.get(1000) == 0);
    int i=0;

    SECTION("3 minute 1000ms") {
        i=0;
        for (; i< MINUTE*3;i=i+1000) {
            airtime.set(i, 1000);
        }
        REQUIRE(airtime.get(i) == 997);
        SECTION("3 minutes 0ms") {
            for (; i< MINUTE*3*2;i=i+1000) {
                airtime.set(i, 0);
            }
            REQUIRE(airtime.get(i) == 532);
        }
    }

    SECTION("3 minute 10ms twice a second") {
        for (; i< MINUTE*3;i=i+500) {
            airtime.set(i, 10);
        }
        REQUIRE(airtime.get(i) == 9);
        SECTION("3 minutes 0ms") {
            for (; i< MINUTE*3*2;i=i+500) {
                airtime.set(i, 0);
            }
            REQUIRE(airtime.get(i) == 0);
        }
    }

    SECTION("1 minute 10ms twice a second") {
        for (; i< MINUTE;i=i+500) {
            airtime.set(i, 10);
        }
        REQUIRE(airtime.get(i) == 9);
        SECTION("1 minutes 0ms") {
            for (; i< MINUTE*2;i=i+500) {
                airtime.set(i, 0);
            }
            REQUIRE(airtime.get(i) == 0);
        }
    }

}