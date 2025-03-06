#include <catch2/catch_test_macros.hpp>
#include "helpers.hpp"

using namespace FANET;

TEST_CASE("Packet ACK", "[single-file]")
{
    Packet<1, 1> pb;
    pb.source(Address{0x123456});
    REQUIRE(pb.buildAck() == makeVector({0x00, 0x12, 0x56, 0x34}));
}


TEST_CASE("Packet ACK Destination", "[single-file]")
{
    Packet<1, 1> pb;
    pb.source(Address{0x123456});
    pb.destination(Address{0x987654});
    REQUIRE(pb.buildAck() == makeVector({0x80, 0x12, 0x56, 0x34, 0x20, 0x98, 0x54, 0x76}));
}

TEST_CASE("Packet ACK geo Forward", "[single-file]")
{
    Packet<1, 1> pb;
    pb.source(Address{0x123456});
    pb.destination(Address{0x987654});
    pb.isGeoForward();
    REQUIRE(pb.buildAck() == makeVector({0x80, 0x12, 0x56, 0x34, 0x21, 0x98, 0x54, 0x76,}));
}

TEST_CASE("Packet ACK isForward", "[single-file]")
{
    Packet<1, 1> pb;
    pb.source(Address{0x123456});
    pb.destination(Address{0x987654});
    pb.forward();
    REQUIRE(pb.buildAck() == makeVector({0xC0, 0x12, 0x56, 0x34, 0x20, 0x98, 0x54, 0x76}));
}

TEST_CASE("Packet ACK signature", "[single-file]")
{
    Packet<1, 1> pb;
    pb.source(Address{0x123456});
    pb.destination(Address{0x987654});
    pb.signature(0x98765432);
    pb.ack(ExtendedHeader::AckType::TWOHOP);
    REQUIRE(pb.buildAck() == makeVector({0x80, 0x12, 0x56, 0x34, 0xB0, 0x98, 0x54, 0x76, 0x32, 0x54, 0x76, 0x98, }));
}

TEST_CASE("Packet Tracking Payload", "[single-file]")
{
    TrackingPayload payload;
    payload.longitude(0);
    payload.latitude(13.4);
    payload.speed(45.6);
    payload.turnRate(6.2);
    Packet<1, 1> pb;
    pb.source(Address{0x123456});
    pb.payload(payload);
    REQUIRE(pb.build() == makeVector({0x01, 0x12, 0x56, 0x34, 0xC0, 0x0E, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5B, 0x00, 0x00, 0x19 }));
}

TEST_CASE("Packet Name Payload", "[single-file]")
{
    NamePayload<34> payload;
    payload.name("Hello World");
    Packet<34, 34> pb;
    pb.source(Address{0x123456});
    pb.payload(payload);
    REQUIRE(pb.build() == makeVector({0x02, 0x12, 0x56, 0x34, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64, }));
}

TEST_CASE("Packet Message Payload", "[single-file]")
{
    MessagePayload<34> payload;
    payload.subHeader(0x56);
    payload.message({0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64});
    Packet<34, 34> pb;
    pb.source(Address{0x123456});
    pb.payload(payload);
    REQUIRE(pb.build() == makeVector({0x03, 0x12, 0x56, 0x34, 0x56, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64 }));
}

TEST_CASE("Packet GroundTracking Payload", "[single-file]")
{
    GroundTrackingPayload payload;
    payload.latitude(52.4123f);
    payload.longitude(-24.6123f);
    payload.groundType(GroundTrackingPayload::TrackingType::NEED_A_RIDE);
    Packet<1, 1> pb;
    pb.source(Address{0x129876});
    pb.payload(payload);
    REQUIRE(pb.build() == makeVector({0x07, 0x12, 0x76, 0x98, 0x95, 0x8A, 0x4A, 0x81, 0x7F, 0xEE, 0x80, }));
}
