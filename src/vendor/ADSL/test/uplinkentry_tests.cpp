
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../include/adsl/errorCorrect.hpp"
#include "../include/adsl/uplinkentry.hpp"

#include "etl/vector.h"
#include "helpers.hpp"

using namespace ADSL;

// ============================================================================
// Default Constructor Tests
// ============================================================================

TEST_CASE("UplinkEntry Default Constructor", "[UplinkEntry]")
{
    UplinkEntry entry;

    REQUIRE(entry.addressMappingTable() == 0x05);
    REQUIRE(entry.address() == Address{});
    REQUIRE(entry.payload().flightState() == TrafficPayload::FlightState::ON_GROUND);
    REQUIRE(entry.payload().aircraftCategory() == TrafficPayload::AircraftCategory::NO_INFO);
}

// ============================================================================
// Single Entry Round-trip
// ============================================================================

TEST_CASE("UplinkEntry single entry serialize/deserialize round-trip", "[UplinkEntry]")
{
    TrafficPayload tp;
    tp.latitude(52.3f);
    tp.longitude(4.8f);
    tp.altitude(1500);
    tp.speed(42.0f);
    tp.groundTrack(270.0f);
    tp.flightState(TrafficPayload::FlightState::AIRBORNE);
    tp.aircraftCategory(TrafficPayload::AircraftCategory::GLIDER_SAILPLANE);
    tp.emergencyStatus(TrafficPayload::EmergencyStatus::NO_EMERGENCY);

    UplinkEntry original;
    original.addressMappingTable(0x06); // FLARM
    original.address(Address{0xABCDEF});
    original.payload(tp);

    auto packet = createRadioPacket([&](etl::bit_stream_writer &writer)
                                    {
        auto bytes = original.serialize(writer);
        REQUIRE(bytes == 19);
        REQUIRE(writer.size_bits() == 152); });

    REQUIRE(packet.size() == 19);

    auto reader = createReader(packet);
    auto received = UplinkEntry::deserialize(reader);

    REQUIRE(received.addressMappingTable() == 0x06);
    REQUIRE(received.address() == Address{0xABCDEF});
    REQUIRE(received.payload().latitude() == Catch::Approx(tp.latitude()).margin(1e-3f));
    REQUIRE(received.payload().longitude() == Catch::Approx(tp.longitude()).margin(1e-3f));
    REQUIRE(received.payload().altitude() == tp.altitude());
    REQUIRE(received.payload().speed() == Catch::Approx(tp.speed()).margin(0.5f));
    REQUIRE(received.payload().groundTrack() == Catch::Approx(tp.groundTrack()).margin(0.7f));
    REQUIRE(received.payload().flightState() == TrafficPayload::FlightState::AIRBORNE);
    REQUIRE(received.payload().aircraftCategory() == TrafficPayload::AircraftCategory::GLIDER_SAILPLANE);
}

// ============================================================================
// Two entries — verify second entry bit placement
// ============================================================================

TEST_CASE("UplinkEntry two entries: second entry bits placed correctly", "[UplinkEntry]")
{
    // First entry: distinctive values
    TrafficPayload tp1;
    tp1.latitude(48.8566f); // Paris
    tp1.longitude(2.3522f);
    tp1.altitude(300);
    tp1.speed(12.0f);
    tp1.groundTrack(45.0f);
    tp1.flightState(TrafficPayload::FlightState::ON_GROUND);
    tp1.aircraftCategory(TrafficPayload::AircraftCategory::LIGHT_FIXED_WING);
    tp1.emergencyStatus(TrafficPayload::EmergencyStatus::NO_EMERGENCY);
    tp1.sourceIntegrityLevel(TrafficPayload::SourceIntegrityLevel::SIL_1E3);
    tp1.designAssurance(TrafficPayload::DesignAssurance::DAL_D);
    tp1.navigationIntegrity(TrafficPayload::NavigationIntegrity::RC_25_75M);
    tp1.horizontalPositionAccuracy(TrafficPayload::HorizontalPositionAccuracy::HFOM_3_10M);
    tp1.verticalPositionAccuracy(TrafficPayload::VerticalPositionAccuracy::VFOM_10_45M);
    tp1.velocityAccuracy(TrafficPayload::VelocityAccuracy::ACC_VEL_1_3MS);

    UplinkEntry entry1;
    entry1.addressMappingTable(0x05); // ICAO
    entry1.address(Address{0x111111});
    entry1.payload(tp1);

    // Second entry: completely different values to ensure no bit bleed
    TrafficPayload tp2;
    tp2.latitude(-33.8688f); // Sydney
    tp2.longitude(151.2093f);
    tp2.altitude(8500);
    tp2.speed(120.0f);
    tp2.groundTrack(180.0f);
    tp2.flightState(TrafficPayload::FlightState::AIRBORNE);
    tp2.aircraftCategory(TrafficPayload::AircraftCategory::PARAGLIDER);
    tp2.emergencyStatus(TrafficPayload::EmergencyStatus::GENERAL_EMERGENCY);
    tp2.sourceIntegrityLevel(TrafficPayload::SourceIntegrityLevel::SIL_1E7);
    tp2.designAssurance(TrafficPayload::DesignAssurance::DAL_B);
    tp2.navigationIntegrity(TrafficPayload::NavigationIntegrity::RC_LESS_7_5M);
    tp2.horizontalPositionAccuracy(TrafficPayload::HorizontalPositionAccuracy::HFOM_LESS_3M);
    tp2.verticalPositionAccuracy(TrafficPayload::VerticalPositionAccuracy::VFOM_LESS_10M);
    tp2.velocityAccuracy(TrafficPayload::VelocityAccuracy::ACC_VEL_LESS_1MS);

    UplinkEntry entry2;
    entry2.addressMappingTable(0x08); // FANET
    entry2.address(Address{0x222222});
    entry2.payload(tp2);

    // Serialize both entries sequentially into one bitstream
    auto packet = createRadioPacket([&](etl::bit_stream_writer &writer)
                                    {
                                        entry1.serialize(writer);
                                        REQUIRE(writer.size_bits() == 152); // entry 1 ends at bit 152
                                        entry2.serialize(writer);
                                        REQUIRE(writer.size_bits() == 304); // entry 2 ends at bit 304 = 2 * 152
                                    });

    REQUIRE(packet.size() == 38); // 2 * 19 bytes

    // Deserialize both entries
    auto reader = createReader(packet);
    auto rx1 = UplinkEntry::deserialize(reader);
    auto rx2 = UplinkEntry::deserialize(reader);

    // ---- Verify first entry ----
    REQUIRE(rx1.addressMappingTable() == 0x05);
    REQUIRE(rx1.address() == Address{0x111111});
    REQUIRE(rx1.payload().latitude() == Catch::Approx(tp1.latitude()).margin(1e-3f));
    REQUIRE(rx1.payload().longitude() == Catch::Approx(tp1.longitude()).margin(1e-3f));
    REQUIRE(rx1.payload().altitude() == tp1.altitude());
    REQUIRE(rx1.payload().speed() == Catch::Approx(tp1.speed()).margin(0.5f));
    REQUIRE(rx1.payload().groundTrack() == Catch::Approx(tp1.groundTrack()).margin(0.7f));
    REQUIRE(rx1.payload().flightState() == TrafficPayload::FlightState::ON_GROUND);
    REQUIRE(rx1.payload().aircraftCategory() == TrafficPayload::AircraftCategory::LIGHT_FIXED_WING);
    REQUIRE(rx1.payload().emergencyStatus() == TrafficPayload::EmergencyStatus::NO_EMERGENCY);
    REQUIRE(rx1.payload().sourceIntegrityLevel() == TrafficPayload::SourceIntegrityLevel::SIL_1E3);
    REQUIRE(rx1.payload().designAssurance() == TrafficPayload::DesignAssurance::DAL_D);
    REQUIRE(rx1.payload().navigationIntegrity() == TrafficPayload::NavigationIntegrity::RC_25_75M);
    REQUIRE(rx1.payload().horizontalPositionAccuracy() == TrafficPayload::HorizontalPositionAccuracy::HFOM_3_10M);
    REQUIRE(rx1.payload().verticalPositionAccuracy() == TrafficPayload::VerticalPositionAccuracy::VFOM_10_45M);
    REQUIRE(rx1.payload().velocityAccuracy() == TrafficPayload::VelocityAccuracy::ACC_VEL_1_3MS);

    // ---- Verify second entry — this is the critical check ----
    // If any bit boundary is wrong, these fields will be corrupted
    REQUIRE(rx2.addressMappingTable() == 0x08);
    REQUIRE(rx2.address() == Address{0x222222});
    REQUIRE(rx2.payload().latitude() == Catch::Approx(tp2.latitude()).margin(1e-3f));
    REQUIRE(rx2.payload().longitude() == Catch::Approx(tp2.longitude()).margin(1e-3f));
    REQUIRE(rx2.payload().altitude() == tp2.altitude());
    REQUIRE(rx2.payload().speed() == Catch::Approx(tp2.speed()).margin(2.5f));
    REQUIRE(rx2.payload().groundTrack() == Catch::Approx(tp2.groundTrack()).margin(0.7f));
    REQUIRE(rx2.payload().flightState() == TrafficPayload::FlightState::AIRBORNE);
    REQUIRE(rx2.payload().aircraftCategory() == TrafficPayload::AircraftCategory::PARAGLIDER);
    REQUIRE(rx2.payload().emergencyStatus() == TrafficPayload::EmergencyStatus::GENERAL_EMERGENCY);
    REQUIRE(rx2.payload().sourceIntegrityLevel() == TrafficPayload::SourceIntegrityLevel::SIL_1E7);
    REQUIRE(rx2.payload().designAssurance() == TrafficPayload::DesignAssurance::DAL_B);
    REQUIRE(rx2.payload().navigationIntegrity() == TrafficPayload::NavigationIntegrity::RC_LESS_7_5M);
    REQUIRE(rx2.payload().horizontalPositionAccuracy() == TrafficPayload::HorizontalPositionAccuracy::HFOM_LESS_3M);
    REQUIRE(rx2.payload().verticalPositionAccuracy() == TrafficPayload::VerticalPositionAccuracy::VFOM_LESS_10M);
    REQUIRE(rx2.payload().velocityAccuracy() == TrafficPayload::VelocityAccuracy::ACC_VEL_LESS_1MS);
}

// ============================================================================
// Bit-size verification
// ============================================================================

TEST_CASE("UplinkEntry serialize produces exactly 152 bits per entry", "[UplinkEntry]")
{
    UplinkEntry entry;
    entry.addressMappingTable(0x3F); // max 6-bit value
    entry.address(Address{0xFFFFFF});

    auto packet = createRadioPacket([&](etl::bit_stream_writer &writer)
                                    {
        entry.serialize(writer);
        REQUIRE(writer.size_bits() == 152); });

    REQUIRE(packet.size() == 19);
}
