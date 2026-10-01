#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <limits>

#include "ace/ownshipstate.hpp"
#include "ace/coreutils.hpp"

using Source = GATAS::PressureSource;
using Origin = GATAS::BarometricSource;

TEST_CASE("Pressure updates gate by origin first then method", "[ownshipstate]")
{
    GATAS::OwnshipState state;
    state.init(spin_lock_instance(0));
    const auto currentOrigin = GENERATE(Origin::Internal, Origin::External);
    const auto incomingOrigin = GENERATE(Origin::Internal, Origin::External);
    const auto currentMethod = GENERATE(Source::PressureSensor, Source::Calculated, Source::Unavailable);
    const auto incomingMethod = GENERATE(Source::PressureSensor, Source::Calculated, Source::Unavailable);
    state.updateBarometricPressure(currentOrigin, {1013.25f, currentMethod}, 1000);
    const auto snapshot = state.loadPressureState();
    state.updateBarometricPressure(incomingOrigin, {898.7628f, incomingMethod}, 2000);
    // Wire method values are 0 unknown, 1 calculated, 2 measured.
    const bool accepted = currentOrigin != incomingOrigin
        ? incomingOrigin == Origin::Internal
        : static_cast<uint8_t>(incomingMethod) >= static_cast<uint8_t>(currentMethod);
    const auto result = state.loadPressureState();
    REQUIRE(result.barometricPressure.size() == 1);
    const auto &sample = result.barometricPressure.begin()->second;
    REQUIRE(result.barometricPressure.begin()->first == (accepted ? incomingMethod : currentMethod));
    REQUIRE(sample.value.source == (accepted ? incomingMethod : currentMethod));
    REQUIRE(sample.value.pressurehPa == (accepted ? 898.7628f : 1013.25f));
    REQUIRE(sample.msSinceEpoch == (accepted ? 2000 : 1000));
    REQUIRE(state.calculatePressureAltitude(2000).value() == Catch::Approx(accepted ? 1000.0f : 0.0f).margin(0.1f));
    REQUIRE(snapshot.barometricPressure.at(currentMethod).value.pressurehPa == 1013.25f);
}

TEST_CASE("Rejected updates do not refresh or cache a lower-priority sample", "[ownshipstate]")
{
    GATAS::OwnshipState state;
    state.init(spin_lock_instance(0));
    constexpr uint64_t now = 1'800'000'000'000;
    state.updateBarometricPressure(Origin::Internal, {1013.25f, Source::Calculated}, now);
    state.updateBarometricPressure(Origin::External, {898.7628f, Source::PressureSensor}, now + 5000);
    REQUIRE(state.loadPressureState().barometricPressure.at(Source::Calculated).msSinceEpoch == now);
    REQUIRE(state.calculatePressureAltitude(now + 5000).has_value());
    REQUIRE_FALSE(state.calculatePressureAltitude(now + 5001));
    // The next update can take over after the accepted internal sample expires.
    state.updateBarometricPressure(Origin::External, {898.7628f, Source::PressureSensor}, now + 5001);
    REQUIRE(state.loadPressureState().barometricPressure.at(Source::PressureSensor).msSinceEpoch == now + 5001);
    REQUIRE(state.calculatePressureAltitude(now + 5001).value() == Catch::Approx(1000.0f).margin(0.1f));
}

TEST_CASE("Lower-quality data within one origin takes over only after expiry", "[ownshipstate]")
{
    GATAS::OwnshipState state;
    state.init(spin_lock_instance(0));
    const auto origin = GENERATE(Origin::Internal, Origin::External);
    state.updateBarometricPressure(origin, {1013.25f, Source::PressureSensor}, 1000);
    state.updateBarometricPressure(origin, {898.7628f, Source::Calculated}, 6000);
    REQUIRE(state.loadPressureState().barometricPressure.count(Source::PressureSensor) == 1);
    state.updateBarometricPressure(origin, {898.7628f, Source::Calculated}, 6001);
    REQUIRE(state.loadPressureState().barometricPressure.count(Source::PressureSensor) == 0);
    REQUIRE(state.loadPressureState().barometricPressure.at(Source::Calculated).msSinceEpoch == 6001);
}

TEST_CASE("Missing pressure invalidates only its accepted origin and method", "[ownshipstate]")
{
    GATAS::OwnshipState state;
    state.init(spin_lock_instance(0));
    const auto origin = GENERATE(Origin::Internal, Origin::External);
    const auto other = origin == Origin::Internal ? Origin::External : Origin::Internal;
    state.updateBarometricPressure(origin, {1013.25f, Source::PressureSensor}, 1000);
    state.updateBarometricPressure(other, {GATAS::INVALID_PRESSSURE_HPA, Source::PressureSensor}, 2000);
    REQUIRE(state.calculatePressureAltitude(2000).has_value());
    state.updateBarometricPressure(origin, {GATAS::INVALID_PRESSSURE_HPA, Source::Calculated}, 2000);
    REQUIRE(state.calculatePressureAltitude(2000).has_value());
    state.updateBarometricPressure(origin, {GATAS::INVALID_PRESSSURE_HPA, Source::PressureSensor}, 2000);
    REQUIRE_FALSE(state.calculatePressureAltitude(2000));
    // Explicit invalidation releases priority for the next valid update.
    state.updateBarometricPressure(other, {898.7628f, Source::Calculated}, 2001);
    REQUIRE(state.calculatePressureAltitude(2001).value() == Catch::Approx(1000.0f).margin(0.1f));
}

TEST_CASE("Pressure altitude uses ambient pressure without QNH or GPS", "[ownshipstate]")
{
    GATAS::OwnshipState state;
    state.init(spin_lock_instance(0));
    REQUIRE(state.loadPressureState().barometricPressure.empty());
    REQUIRE_FALSE(state.calculatePressureAltitude(100));
    const auto source = GENERATE(Source::PressureSensor, Source::Calculated, Source::Unavailable);
    struct Case { float hpa; float metres; };
    const Case cases[] = {{1013.25f, 0.0f}, {898.7628f, 1000.0f}, {1025.0f, -97.4f}};
    for (const auto &test : cases)
    {
        state.updateBarometricPressure(Origin::Internal, {test.hpa, source}, 100);
        REQUIRE(state.calculatePressureAltitude(100).value() == Catch::Approx(test.metres).margin(0.1f));
    }
    GATAS::OwnshipPositionInfo location{};
    location.timestamp = 123;
    location.ellipseHeight = 450;
    state.location.store(location);
    for (const float invalid : {GATAS::INVALID_PRESSSURE_HPA, 0.0f,
                                std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity()})
    {
        state.updateBarometricPressure(Origin::Internal, {invalid, source}, 100);
        REQUIRE_FALSE(state.calculatePressureAltitude(100));
        REQUIRE(state.location.load().ellipseHeight == 450);
    }
}

TEST_CASE("Pressure expiry handles epoch clock corrections", "[ownshipstate]")
{
    GATAS::OwnshipState state;
    state.init(spin_lock_instance(0));
    constexpr uint64_t now = 1'800'000'000'000;
    state.updateBarometricPressure(Origin::Internal, {1013.25f, Source::PressureSensor}, now);
    REQUIRE_FALSE(state.calculatePressureAltitude(now - 1));
    REQUIRE_FALSE(state.calculatePressureAltitude(now + 1'000'000));
    state.updateBarometricPressure(Origin::External, {898.7628f, Source::Calculated}, now - 100'000);
    REQUIRE(state.calculatePressureAltitude(now - 100'000).value() == Catch::Approx(1000.0f).margin(0.1f));
}
