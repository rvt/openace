
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../fanet/utils.hpp"
#include "../fanet/tracking.hpp"

using namespace FANET;

#define private public

template <class T>
const T &constrain(const T &x, const T &lo, const T &hi)
{
    if (x < lo)
        return lo;
    else if (hi < x)
        return hi;
    else
        return x;
}

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



int16_t climbRate_Origional(float climbRate)
{
    int climb10 = constrain((int)std::round(climbRate * 10.0f), -315, 315);
    if (std::abs(climb10) > 63)
        return ((climb10 + (climb10 >= 0 ? 2 : -2)) / 5);
    else
        return climb10;
}

TEST_CASE("toScaled climbRate", "[Utils]")
{
    auto result = toScaled<int16_t, etl::ratio<1, 10>, etl::ratio<1, 2>, 7>(-2.5f);
    REQUIRE(result.scaled == false);
    REQUIRE(result.value == climbRate_Origional(-2.5f));

    result = toScaled<int16_t, etl::ratio<1, 10>, etl::ratio<1, 2>, 7>(-20.5f);
    REQUIRE(result.scaled == true);
    REQUIRE(result.value == climbRate_Origional(-20.5f));
}

int16_t turnRate_Origional(float turnRate)
{
    int trOs = constrain((int)std::round(turnRate * 4.0f), -254, 254);
    if (std::abs(trOs) >= 63)
        return ((trOs + (trOs >= 0 ? 2 : -2)) / 4);
    else
        return trOs;
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

int16_t speed_Origional(float speed)
{
    int speed2 = constrain((int)std::round(speed * 2.0f), 0, 635);
    if (speed2 > 127)    
        return ((speed2 + 2) / 5);
    else
        return speed2;
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

int16_t altitude_Origional(float altitude)
{
    int alt = constrain(altitude, 0.f, 8190.f);
    if(alt > 2047)
        return (alt+2)/4;
    else
        return alt;    
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


