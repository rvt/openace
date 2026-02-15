
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../include/adsl/trafficpayload.hpp"

#include "etl/vector.h"
#include "helpers.hpp"

using namespace ADSL;

// ============================================================================
// Default Constructor Tests
// ============================================================================

TEST_CASE("TrafficPayload Default Constructor", "[TrafficPayload]")
{
  TrafficPayload payload;

  //    REQUIRE(payload.type() == Header::MessageType::TRACKING);
  REQUIRE(payload.latitude() == Catch::Approx(-90.00073f).margin(0.000001));
  REQUIRE(payload.longitude() == Catch::Approx(-180.00146f).margin(0.000001));
  REQUIRE(payload.altitude() == 61116);
  REQUIRE(payload.speed() == Catch::Approx(239.f).margin(0.125f));
  REQUIRE(payload.verticalRate() == Catch::Approx(-119.5f).margin(0.1));
  REQUIRE(payload.groundTrack() == 0);

  REQUIRE(payload.flightState() == TrafficPayload::FlightState::ON_GROUND);
  REQUIRE(payload.aircraftCategory() == TrafficPayload::AircraftCategory::NO_INFO);
  REQUIRE(payload.emergencyStatus() == TrafficPayload::EmergencyStatus::NO_EMERGENCY);
  REQUIRE(payload.sourceIntegrityLevel() == TrafficPayload::SourceIntegrityLevel::UNDEFINED_SIL);
  REQUIRE(payload.designAssurance() == TrafficPayload::DesignAssurance::UNDEFINED_NONE);
  REQUIRE(payload.navigationIntegrity() == TrafficPayload::NavigationIntegrity::UNDEFINED);
  REQUIRE(payload.horizontalPositionAccuracy() == TrafficPayload::HorizontalPositionAccuracy::UNKNOWN_NO_FIX);
  REQUIRE(payload.verticalPositionAccuracy() == TrafficPayload::VerticalPositionAccuracy::UNKNOWN_NO_FIX);
  REQUIRE(payload.velocityAccuracy() == TrafficPayload::VelocityAccuracy::UNKNOWN_NO_FIX);
  REQUIRE(payload.timestampQms() == 0);
}

TEST_CASE("TrafficPayload Serialize/Deserialize Roundtrip Values", "[TrafficPayload]")
{
  TrafficPayload layer;
  layer.latitude(50.0f);
  layer.longitude(4.0f);
  layer.speed(12.5f);
  layer.altitude(12876);
  layer.verticalRate(13.2f);
  layer.groundTrack(123.4f);
  layer.flightState(TrafficPayload::FlightState::ALTITUDE_SOURCE_INVALID);
  layer.aircraftCategory(TrafficPayload::AircraftCategory::UAS_OPEN_CATEGORY);
  layer.emergencyStatus(TrafficPayload::EmergencyStatus::UNLAWFUL_INTERFERENCE);
  layer.sourceIntegrityLevel(TrafficPayload::SourceIntegrityLevel::SIL_1E5);
  layer.designAssurance(TrafficPayload::DesignAssurance::DAL_C);
  layer.navigationIntegrity(TrafficPayload::NavigationIntegrity::RC_25_75M);
  layer.horizontalPositionAccuracy(TrafficPayload::HorizontalPositionAccuracy::HFOM_3_10M);
  layer.verticalPositionAccuracy(TrafficPayload::VerticalPositionAccuracy::VFOM_10_45M);
  layer.velocityAccuracy(TrafficPayload::VelocityAccuracy::ACC_VEL_1_3MS);

  auto packet = createRadioPacket([&](etl::bit_stream_writer &writer)
                                  { 
    layer.serialize_issue1(writer); 
     REQUIRE( 120 == writer.size_bits()); });
  auto reader = createReader(packet);
  auto received = TrafficPayload::deserialize_issue1(reader);

  // Check numeric fields with reasonable tolerances
  REQUIRE(received.timestampQms() == layer.timestampQms());
  REQUIRE(received.flightState() == layer.flightState());
  REQUIRE(received.aircraftCategory() == layer.aircraftCategory());
  REQUIRE(received.emergencyStatus() == layer.emergencyStatus());

  REQUIRE(received.latitude() == Catch::Approx(layer.latitude()).margin(1e-3f));
  REQUIRE(received.longitude() == Catch::Approx(layer.longitude()).margin(1e-3f));
  REQUIRE(received.speed() == Catch::Approx(layer.speed()).margin(1e-2f));
  REQUIRE(received.altitude() == layer.altitude());
  REQUIRE(received.verticalRate() == Catch::Approx(layer.verticalRate()).margin(1e-2f));
  REQUIRE(received.groundTrack() == Catch::Approx(layer.groundTrack()).margin(1e-2f));

  REQUIRE(received.sourceIntegrityLevel() == layer.sourceIntegrityLevel());
  REQUIRE(received.designAssurance() == layer.designAssurance());
  REQUIRE(received.navigationIntegrity() == layer.navigationIntegrity());
  REQUIRE(received.horizontalPositionAccuracy() == layer.horizontalPositionAccuracy());
  REQUIRE(received.verticalPositionAccuracy() == layer.verticalPositionAccuracy());
  REQUIRE(received.velocityAccuracy() == layer.velocityAccuracy());
}

TEST_CASE("TrafficPayload Latitude ", "[single-file]")
{
  TrafficPayload payload;
  payload.latitude(56.95812f);
  REQUIRE(payload.latitude() == Catch::Approx(56.95812f).margin(0.000001));
  payload.latitude(-56.18748);
  REQUIRE(payload.latitude() == Catch::Approx(-56.18748f).margin(0.000001));
  payload.latitude(-89.999);
  REQUIRE(payload.latitude() == Catch::Approx(-89.999).margin(0.000001));
  payload.latitude(89.999);
  REQUIRE(payload.latitude() == Catch::Approx(89.999).margin(0.000001));
}

TEST_CASE("TrafficPayload Longitude ", "[single-file]")
{
  TrafficPayload payload;
  payload.longitude(160.54197);
  REQUIRE(payload.longitude() == Catch::Approx(160.54197).margin(0.00002));
  payload.longitude(-126.74510);
  REQUIRE(payload.longitude() == Catch::Approx(-126.74510).margin(0.00002));
  payload.longitude(-179.999);
  REQUIRE(payload.longitude() == Catch::Approx(-180).margin(0.00002));
  payload.longitude(179.999);
  REQUIRE(payload.longitude() == Catch::Approx(180).margin(0.00002));
}

TEST_CASE("TrafficPayload altitude ", "[single-file]")
{
  TrafficPayload payload;

  payload.altitude(-400);
  REQUIRE(payload.altitude() == -320);

  payload.altitude(62000);
  REQUIRE(payload.altitude() == 61116);

  payload.altitude(0);
  REQUIRE(payload.altitude() == 0);

  payload.altitude(12000);
  REQUIRE(payload.altitude() == 12002);
}

TEST_CASE("TrafficPayload speed ", "[single-file]")
{
  TrafficPayload payload;
  payload.speed(0);
  REQUIRE(payload.speed() == Catch::Approx(0).margin(0.5));

  payload.speed(20.0);
  REQUIRE(payload.speed() == Catch::Approx(20).margin(0.5));

  payload.speed(100.0);
  REQUIRE(payload.speed() == Catch::Approx(100.0).margin(2.5));

  payload.speed(237);
  REQUIRE(payload.speed() == Catch::Approx(236).margin(2.5));
}

TEST_CASE("TrafficPayload verticalrate ", "[single-file]")
{
  TrafficPayload payload;
  payload.verticalRate(-125.0f);
  REQUIRE(payload.verticalRate() == Catch::Approx(-118.5f).margin(0.125 / 2));

  payload.verticalRate(125.0f);
  REQUIRE(payload.verticalRate() == Catch::Approx(119.5).margin(0.125 / 2));

  payload.verticalRate(-10.125f);
  REQUIRE(payload.verticalRate() == Catch::Approx(-10.125).margin(0.125 / 2));
  payload.verticalRate(5.8f);
  REQUIRE(payload.verticalRate() == Catch::Approx(5.75f).margin(0.125 / 2));
}

TEST_CASE("TrafficPayload groundTrack ", "[single-file]")
{
  TrafficPayload payload;
  payload.groundTrack(0.0f);
  REQUIRE(payload.groundTrack() == Catch::Approx(0.0f).margin(0.7));

  payload.groundTrack(360.0f);
  REQUIRE(payload.groundTrack() == Catch::Approx(0.0).margin(0.7));

  payload.groundTrack(78.0f);
  REQUIRE(payload.groundTrack() == Catch::Approx(78.0).margin(0.7));

  payload.groundTrack(-5.0f);
  REQUIRE(payload.groundTrack() == Catch::Approx(355.0).margin(0.7));

  payload.groundTrack(370.0f);
  REQUIRE(payload.groundTrack() == Catch::Approx(10.0).margin(0.7));
}

TEST_CASE("TrafficPayload integrity ", "[single-file]")
{
  TrafficPayload payload;
  payload.sourceIntegrityLevel(TrafficPayload::SourceIntegrityLevel::SIL_1E5);
  REQUIRE(payload.sourceIntegrityLevel() == TrafficPayload::SourceIntegrityLevel::SIL_1E5);

  payload.designAssurance(TrafficPayload::DesignAssurance::DAL_C);
  REQUIRE(payload.designAssurance() == TrafficPayload::DesignAssurance::DAL_C);

  payload.navigationIntegrity(TrafficPayload::NavigationIntegrity::RC_75M_0_1NM);
  REQUIRE(payload.navigationIntegrity() == TrafficPayload::NavigationIntegrity::RC_75M_0_1NM);

  payload.horizontalPositionAccuracy(TrafficPayload::HorizontalPositionAccuracy::HFOM_30M_0_05NM);
  REQUIRE(payload.horizontalPositionAccuracy() == TrafficPayload::HorizontalPositionAccuracy::HFOM_30M_0_05NM);

  payload.verticalPositionAccuracy(TrafficPayload::VerticalPositionAccuracy::VFOM_10_45M);
  REQUIRE(payload.verticalPositionAccuracy() == TrafficPayload::VerticalPositionAccuracy::VFOM_10_45M);

  payload.velocityAccuracy(TrafficPayload::VelocityAccuracy::ACC_VEL_1_3MS);
  REQUIRE(payload.velocityAccuracy() == TrafficPayload::VelocityAccuracy::ACC_VEL_1_3MS);

  payload.aircraftCategory(TrafficPayload::AircraftCategory::MODEL_PLANE);
  REQUIRE(payload.aircraftCategory() == TrafficPayload::AircraftCategory::MODEL_PLANE);

  payload.emergencyStatus(TrafficPayload::EmergencyStatus::LIFEGUARD_MEDICAL);
  REQUIRE(payload.emergencyStatus() == TrafficPayload::EmergencyStatus::LIFEGUARD_MEDICAL);

  payload.flightState(TrafficPayload::FlightState::AIRBORNE);
  REQUIRE(payload.flightState() == TrafficPayload::FlightState::AIRBORNE);
}

TEST_CASE("TrafficPayload serialize", "[single-file]")
{
  TrafficPayload payload;
  auto packet = createRadioPacket([&](etl::bit_stream_writer &writer)
                                  { payload.serialize_issue1(writer); });

  // printBufferHex(etl::span(packet.data(), packet.size()));
  REQUIRE(packet == makeVector({0x00, 0x04, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00}));
}

TEST_CASE("Timestamp Restore Should not round up, received in future", "single-file")
{
  TrafficPayload payload;
  auto timestamp = 1771009961749ULL;
  payload.timestamp(timestamp);

  REQUIRE(1771009961500LL == payload.timestampRestored(timestamp + 5600));
}

TEST_CASE("Timestamp Restore restore in past", "single-file")
{
  TrafficPayload payload;
  auto timestamp = 1771009961749ULL;
  payload.timestamp(timestamp);

  REQUIRE(1771009961500LL == payload.timestampRestored(timestamp - 2000));
}

