
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../include/adsl/protocol.hpp"
#include "etl/vector.h"
#include "helpers.hpp"
#include <vector>

using namespace ADSL;

// // Test connector that captures objects passed via the Connector interface
etl::array<uint32_t, 40> localMemory{};
etl::span<uint32_t> bufferSpan(localMemory.data(), localMemory.size());

etl::array<uint32_t, 40> newMemory{};
etl::span<uint32_t> newBufferSpan(localMemory.data(), localMemory.size());
struct TestConnector : public Connector
{
    uint32_t tick = 1000;
    FrameBuffer lastFrame{bufferSpan};
    TrafficPayload lastTrafficPayload{};
    bool trafficSet = false;
    StatusPayload lastStatusPayload{};
    bool statusSet = false;
    Header lastHeader{};
    bool headerSet = false;
    const void *lastCtx;

    uint32_t adsl_getTick() const override { return tick; }

    virtual bool adsl_sendFrame(const void *ctx, uint8_t const *data, size_t lengthBytes) override
    {
        lastFrame.assign(etl::span<const uint8_t>(data, lengthBytes));
        lastFrame.used_bytes = lengthBytes;
        lastCtx = ctx;
        return true;
    }

    virtual void adsl_receivedTraffic(const Header &header, const TrafficPayload &tp) override
    {
        lastTrafficPayload = tp;
        trafficSet = true;
        lastHeader = header;
        headerSet = true;
    }

    virtual void adsl_receivedStatus(const ADSL::Header &header, const StatusPayload &sp) override
    {
        lastStatusPayload = sp;
        statusSet = true;
        lastHeader = header;
        headerSet = true;
    }

    virtual void adsl_buildTraffic(const void *ctx, TrafficPayload &tp) override
    {
        tp = lastTrafficPayload;
    }

    virtual void adsl_buildStatusPayload(const void *ctx, StatusPayload &tp) override
    {
    }

    virtual uint8_t *adsl_alloc(const void *ctx, size_t sizeBytes) override
    {
        static uint8_t memoryPool[256];
        return memoryPool;
    }
};
const auto buffer = etl::make_array<uint32_t>(0xB229CC00, 0x9981C4E4, 0x5DD2E995, 0x20492D0B, 0xBD66A043, 0xEE82CD94);

// // // ============================================================================
// // // Default Constructor Tests
// // // ============================================================================

TEST_CASE("Protocol handleRx", "[Protocol]")
{
    TestConnector conn;
    Protocol protocol(&conn);
    protocol.crcCheckOnReceive = true;

    // Actual generated and validated ADSL packet
    auto newBuffer = buffer;
    FrameBuffer buf{newBuffer.data(), newBuffer.size()};
    protocol.crcCheckOnReceive = true;
    auto result = protocol.handleRx(-42, buf.fullSpan32());

    REQUIRE(result == Protocol::RxStatudeCode::OK);

    // Validate header fields
    REQUIRE(conn.headerSet);
    REQUIRE(conn.lastHeader.type() == Header::PayloadTypeIdentifier::TRAFFIC);
    REQUIRE(conn.lastHeader.addressMappingTable() == 0x05);
    REQUIRE(conn.lastHeader.address().asUint() == 0xD91A6E);
    REQUIRE(conn.lastHeader.forward() == false);

    // Validate parsed traffic payload fields
    REQUIRE(conn.trafficSet);
    REQUIRE(conn.lastTrafficPayload.timestampQms() == 10);
    REQUIRE(conn.lastTrafficPayload.flightState() == TrafficPayload::FlightState::AIRBORNE);
    REQUIRE(conn.lastTrafficPayload.aircraftCategory() == TrafficPayload::AircraftCategory::LIGHT_FIXED_WING);
    REQUIRE(conn.lastTrafficPayload.emergencyStatus() == TrafficPayload::EmergencyStatus::NO_EMERGENCY);
    REQUIRE(Catch::Approx(54.107014) == conn.lastTrafficPayload.latitude());
    REQUIRE(Catch::Approx(3.856962) == conn.lastTrafficPayload.longitude());
    REQUIRE(Catch::Approx(55.50).epsilon(0.01) == conn.lastTrafficPayload.speed());
    REQUIRE(Catch::Approx(184.92).epsilon(0.01) == conn.lastTrafficPayload.groundTrack());
    REQUIRE(conn.lastTrafficPayload.altitude() == 7685);
    REQUIRE(Catch::Approx(-3.25).epsilon(0.01) == conn.lastTrafficPayload.verticalRate());
    REQUIRE(conn.lastTrafficPayload.sourceIntegrityLevel() == TrafficPayload::SourceIntegrityLevel::UNDEFINED_SIL);
    REQUIRE(conn.lastTrafficPayload.designAssurance() == TrafficPayload::DesignAssurance::UNDEFINED_NONE);
    REQUIRE(conn.lastTrafficPayload.navigationIntegrity() == TrafficPayload::NavigationIntegrity::RC_7_5_25M);
    REQUIRE(conn.lastTrafficPayload.horizontalPositionAccuracy() == TrafficPayload::HorizontalPositionAccuracy::HFOM_LESS_3M);
    REQUIRE(conn.lastTrafficPayload.verticalPositionAccuracy() == TrafficPayload::VerticalPositionAccuracy::VFOM_LESS_10M);
    REQUIRE(conn.lastTrafficPayload.velocityAccuracy() == TrafficPayload::VelocityAccuracy::ACC_VEL_LESS_1MS);
}

TEST_CASE("Protocol rqSendTrafficPayload", "[Protocol]")
{
    TestConnector conn;
    Protocol protocol(&conn);
    protocol.crcCheckOnReceive = true;

    auto newBuffer = buffer;
    FrameBuffer buf{newBuffer.data(), newBuffer.size()};
    auto result = protocol.handleRx(-42, buf.fullSpan32());
    REQUIRE(result == Protocol::RxStatudeCode::OK);

    protocol.rqSendTrafficPayload((void *)0x1234);

    conn.trafficSet = false;
    result = protocol.handleRx(-42, conn.lastFrame.usedSpan32());
    REQUIRE(result == Protocol::RxStatudeCode::OK);
    REQUIRE(conn.trafficSet);
    REQUIRE(conn.lastCtx == (void *)0x1234);
}

TEST_CASE("Protocol validation", "[Protocol]")
{
    TestConnector conn;
    Protocol protocol(&conn);
    Header header;
    NetworkPayload network;
    StatusPayload status;
    TrafficPayload traffic;

    protocol.crcCheckOnReceive = true;
    SECTION("OK")
    {
        header.type(Header::PayloadTypeIdentifier::STATUS);
        auto frame = buildRadioPacket(newBufferSpan, header, network, [&](etl::bit_stream_writer &writer)
                                      { status.serialize_issue2(writer); });

        auto result = protocol.handleRx(-42, frame.usedSpan32());
        REQUIRE(result == Protocol::RxStatudeCode::OK);
    }

    SECTION("UNSUPPORTED_PAYLOAD")
    {
        header.type(Header::PayloadTypeIdentifier::REMOTEID);
        auto frame = buildRadioPacket(bufferSpan, header, network, [&](etl::bit_stream_writer &writer)
                                      { traffic.serialize_issue1(writer); });

        auto result = protocol.handleRx(-42, frame.usedSpan32());
        REQUIRE(result == Protocol::RxStatudeCode::UNSUPPORTED_PAYLOAD);
    }

    SECTION("CRC Error")
    {
        header.type(Header::PayloadTypeIdentifier::REMOTEID);
        auto frame = buildRadioPacket(bufferSpan, header, network, [&](etl::bit_stream_writer &writer)
                                      { traffic.serialize_issue1(writer); });

        frame[23] = 0; // Set incorrect CRC
        auto result = protocol.handleRx(-42, frame.usedSpan32());
        REQUIRE(result == Protocol::RxStatudeCode::CRC_FAILED);
    }

    SECTION("Decryption key 1 unsupported")
    {
        network.keyIndex(1);
        auto frame = buildRadioPacket(bufferSpan, header, network, [&](etl::bit_stream_writer &writer)
                                      { status.serialize_issue2(writer); });

        auto result = protocol.handleRx(-42, frame.usedSpan32());
        REQUIRE(result == Protocol::RxStatudeCode::UNSUPORTED_DECRYPTION_KEY);
    }

    SECTION("Decryption key 3 supported")
    {
        header.type(Header::PayloadTypeIdentifier::STATUS);
        network.keyIndex(3);
        auto frame = buildRadioPacket(bufferSpan, header, network, [&](etl::bit_stream_writer &writer)
                                      { status.serialize_issue2(writer); });

        auto result = protocol.handleRx(-42, frame.usedSpan32());
        REQUIRE(result == Protocol::RxStatudeCode::OK);
    }

    SECTION("FEC Not supported")
    {
        network.errorControlMode(NetworkPayload::ErrorControlMode::FEC);
        auto frame = buildRadioPacket(bufferSpan, header, network, [&](etl::bit_stream_writer &writer)
                                      { status.serialize_issue2(writer); });

        auto result = protocol.handleRx(-42, frame.usedSpan32());
        REQUIRE(result == Protocol::RxStatudeCode::UNSUPORTED_ERROR_CONTROL_FEC);
    }

    SECTION("Unsupported protocol version")
    {
        network.protocolVersion(static_cast<NetworkPayload::ProtocolVersion>(3));
        auto frame = buildRadioPacket(bufferSpan, header, network, [&](etl::bit_stream_writer &writer)
                                      { status.serialize_issue2(writer); });

        auto result = protocol.handleRx(-42, frame.usedSpan32());
        REQUIRE(result == Protocol::RxStatudeCode::UNSUPORTED_PROTOCOL_VERSION);
    }
}

TEST_CASE("Protocol Version Default", "[Protocol]")
{
    TestConnector conn;
    Protocol protocol(&conn);
    REQUIRE(protocol.lowestDominotorProtocolVersion() == NetworkPayload::ProtocolVersion::LATEST);
}

TEST_CASE("Protocol Version With Traffic", "[Protocol]")
{
    struct Case
    {
        float lat;
        float lon;
        int alt;
        float speed;
        float track;
        bool sendStatus;
        NetworkPayload::ProtocolVersion statusVersion;
        NetworkPayload::ProtocolVersion trafficVersion;
        NetworkPayload::ProtocolVersion expected;
        const char *name;
    };

    auto tc = GENERATE(

        Case{
            .lat = 53.26f,
            .lon = 4.26f,
            .alt = 6000, // Δalt = 1000 m
            .speed = 50.0f,
            .track = 45.f,
            .sendStatus = false,
            .statusVersion = NetworkPayload::ProtocolVersion::LATEST,
            .trafficVersion = NetworkPayload::ProtocolVersion::LATEST,
            .expected = NetworkPayload::ProtocolVersion::LATEST,
            .name = "Vertical ≥ 800 m blocks downgrade"},

        Case{
            .lat = 53.40f,
            .lon = 4.60f, // far horizontally
            .alt = 5500,  // Δalt = 500 m
            .speed = 50.0f,
            .track = 45.f,
            .sendStatus = false,
            .statusVersion = NetworkPayload::ProtocolVersion::LATEST,
            .trafficVersion = NetworkPayload::ProtocolVersion::LATEST,
            .expected = NetworkPayload::ProtocolVersion::LATEST,
            .name = "Vertical < 800 m but no distance or TCPA trigger"},

        Case{
            .lat = 53.255f,
            .lon = 4.255f, // ~647 m horizontally
            .alt = 5500,   // Δalt = 500 m
            .speed = 50.0f,
            .track = 45.f,
            .sendStatus = false,
            .statusVersion = NetworkPayload::ProtocolVersion::ISSUE_1,
            .trafficVersion = NetworkPayload::ProtocolVersion::ISSUE_1,
            .expected = NetworkPayload::ProtocolVersion::ISSUE_1,
            .name = "Distance < 1 km triggers downgrade"},

        Case{
            .lat = 53.26f,
            .lon = 4.25f, // 1.1Km distance
            .alt = 5500,
            .speed = 50.0f,
            .track = 45.f, // same track as traffic → no closure
            .sendStatus = false,
            .statusVersion = NetworkPayload::ProtocolVersion::LATEST,
            .trafficVersion = NetworkPayload::ProtocolVersion::LATEST,
            .expected = NetworkPayload::ProtocolVersion::LATEST,
            .name = "Parallel flight → TCPA 1569 → no downgrade"},

        Case{
            .lat = 53.268f,
            .lon = 4.25f, // 3Km distance
            .alt = 5500,
            .speed = 50.f,
            .track = 135.f, // opposite of traffic (45° + 180)
            .sendStatus = false,
            .statusVersion = NetworkPayload::ProtocolVersion::ISSUE_1,
            .trafficVersion = NetworkPayload::ProtocolVersion::ISSUE_1,
            .expected = NetworkPayload::ProtocolVersion::ISSUE_1,
            .name = "Head-on closure → TCPA < 30 s triggers downgrade"},

        Case{
            .lat = 53.268f,
            .lon = 4.25f, // 3Km distance
            .alt = 5500,
            .speed = 50.f,
            .track = 135.f, // opposite of traffic (45° + 180)
            .sendStatus = true,
            .statusVersion = NetworkPayload::ProtocolVersion::ISSUE_2,
            .trafficVersion = NetworkPayload::ProtocolVersion::ISSUE_1,
            .expected = NetworkPayload::ProtocolVersion::ISSUE_2,
            // This happens when there was a other aircraft in the vicinity
            // and they transmit at ISSUE_1 byt GATAS did not see that one yet.
            .name = "Head-on closure → TCPA < 30 s triggers ISSUE_2 no downgrade ISSUE_2"},

        Case{
            .lat = 53.35f,
            .lon = 4.35f,
            .alt = 5500,
            .speed = 200.f,
            .track = 35.f, // same direction, faster but opening
            .sendStatus = false,
            .statusVersion = NetworkPayload::ProtocolVersion::LATEST,
            .trafficVersion = NetworkPayload::ProtocolVersion::LATEST,
            .expected = NetworkPayload::ProtocolVersion::LATEST,
            .name = "Diverging flight → TCPA invalid → no downgrade"});

    TestConnector conn;
    Protocol protocol(&conn);

    INFO(tc.name);
    protocol.tick(tc.lat, tc.lon, tc.alt, tc.speed, tc.track);

    if (tc.sendStatus)
    {
        NetworkPayload network;
        network.protocolVersion(tc.statusVersion);
        Header header;
        header.address(Address{0x123456});
        header.type(Header::PayloadTypeIdentifier::STATUS);
        StatusPayload status;
        status.maxProtocolversion(tc.statusVersion);
        auto frame = buildRadioPacket(bufferSpan, header, network, [&](etl::bit_stream_writer &writer)
                                      { status.serialize_issue2(writer); });
        auto result = protocol.handleRx(-42, frame.usedSpan32());
        REQUIRE(result == Protocol::RxStatudeCode::OK);
    }

    NetworkPayload network;
    network.protocolVersion(tc.trafficVersion);
    Header header;
    header.type(Header::PayloadTypeIdentifier::TRAFFIC);
    header.address(Address{0x123456});
    TrafficPayload traffic;
    traffic.latitude(53.25f);
    traffic.longitude(4.25f);
    traffic.altitude(5000);
    traffic.speed(50);
    traffic.groundTrack(45);

    auto frame = buildRadioPacket(bufferSpan, header, network, [&](etl::bit_stream_writer &writer)
                                  { traffic.serialize_issue1(writer); });
    auto result = protocol.handleRx(-42, frame.usedSpan32());

    REQUIRE(result == Protocol::RxStatudeCode::OK);
    REQUIRE(protocol.lowestDominotorProtocolVersion() == tc.expected);
}

TEST_CASE("Protocol Version expiry", "[Protocol]")
{
    using NP = NetworkPayload::ProtocolVersion;

    struct Case
    {
        uint32_t address;
        NetworkPayload::ProtocolVersion maxVersion;
        NetworkPayload::ProtocolVersion networkVersion;
        uint32_t time;
        uint32_t expectedAircraft;
        NetworkPayload::ProtocolVersion expected;
        const char *name;
    };

    auto traffic = etl::make_array<Case>(
        Case{.address = 0x111111, .maxVersion = NP::ISSUE_2, .networkVersion = NP::ISSUE_2, .time = 0'000'000, .expectedAircraft = 1, .expected = NP::ISSUE_2, .name = "Aircraft 1"},
        Case{.address = 0x111111, .maxVersion = NP::ISSUE_2, .networkVersion = NP::ISSUE_2, .time = 10'000'000, .expectedAircraft = 1, .expected = NP::ISSUE_2, .name = "Aircraft 1 again"},
        Case{.address = 0x222222, .maxVersion = NP::ISSUE_1, .networkVersion = NP::ISSUE_2, .time = 30'000'000, .expectedAircraft = 2, .expected = NP::ISSUE_1, .name = "Aircraft lower protcol appears"},
        Case{.address = 0x333333, .maxVersion = NP::ISSUE_2, .networkVersion = NP::ISSUE_2, .time = 05'000'000, .expectedAircraft = 3, .expected = NP::ISSUE_1, .name = "Other aircraft "},
        Case{.address = 0x333333, .maxVersion = NP::ISSUE_2, .networkVersion = NP::ISSUE_2, .time = 20'000'000, .expectedAircraft = 2, .expected = NP::ISSUE_1, .name = "Other aircraft again"},
        Case{.address = 0x333333, .maxVersion = NP::ISSUE_2, .networkVersion = NP::ISSUE_2, .time = 20'000'000, .expectedAircraft = 1, .expected = NP::ISSUE_2, .name = "0x333333"},
        Case{.address = 0x444444, .maxVersion = NP::ISSUE_1, .networkVersion = NP::ISSUE_1, .time = 20'000'000, .expectedAircraft = 2, .expected = NP::ISSUE_2, .name = "one new aircraft"},
        Case{.address = 0x444444, .maxVersion = NP::ISSUE_1, .networkVersion = NP::ISSUE_1, .time = 20'000'000, .expectedAircraft = 1, .expected = NP::ISSUE_2, .name = "one aircraft"});

    TestConnector conn;
    Protocol protocol(&conn);

    for (auto const &t : traffic)
    {
        INFO("Case: " << t.name);
        CAPTURE(t.address);
        CAPTURE(t.time);
        CAPTURE(static_cast<int>(t.expectedAircraft));
        CAPTURE(static_cast<int>(t.expected));

        conn.tick += t.time;
        protocol.tick(53.268f, 4.25f, 5500, 50.f, 45.f);

        NetworkPayload network;
        network.protocolVersion(NP::ISSUE_2);

        Header header;
        header.address(Address{t.address});
        header.type(Header::PayloadTypeIdentifier::STATUS);

        StatusPayload status;
        status.maxProtocolversion(NP::ISSUE_2);

        auto frame = buildRadioPacket(bufferSpan, header, network,
                                      [&](etl::bit_stream_writer &writer)
                                      {
                                          status.serialize_issue2(writer);
                                      });

        auto result = protocol.handleRx(-42, frame.usedSpan32());

        REQUIRE(result == Protocol::RxStatudeCode::OK);
        REQUIRE(protocol.numberOfNeighbours() == t.expectedAircraft);
    }
}

// TODO: Add test to check if status payload will be send