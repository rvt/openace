#include <catch2/catch_test_macros.hpp>
#include "../fanet/packetParser.hpp"
#include "helpers.hpp"
#include <catch2/catch_approx.hpp>

using namespace FANET;


TEST_CASE("PacketBuilder ACK ", "[single-file]")
{
    auto packet = PacketParser<100, 100>::parse(makeVector({0x80, 0x12, 0x56, 0x34, 0x20, 0x98, 0x54, 0x76}));
    REQUIRE(packet.header().type() == Header::MessageType::ACK);
    REQUIRE(packet.header().forward() == false);
    REQUIRE(packet.source().asUint() == 0x123456);
    REQUIRE(packet.destination().value().asUint() == 0x987654);
    REQUIRE(packet.extendedHeader().value().unicast() == true);
    REQUIRE(packet.extendedHeader().value().geoForward() == false);
    REQUIRE(packet.extendedHeader().value().unicast() == true);
    REQUIRE(packet.extendedHeader().value().signature() == false);
    REQUIRE(!packet.signature());
    REQUIRE(!packet.payload());
}

TEST_CASE("PacketBuilder forward", "[single-file]")
{
    auto packet = PacketParser<100, 100>::parse(makeVector({0xC0, 0x12, 0x56, 0x34, 0x20, 0x98, 0x54, 0x76}));
    REQUIRE(packet.header().forward() == true);
}

TEST_CASE("PacketBuilder extendedHeader geo forward", "[single-file]")
{
    auto packet = PacketParser<100, 100>::parse(makeVector({0x80, 0x12, 0x56, 0x34, 0x21, 0x98, 0x54, 0x76,}));
    REQUIRE(packet.extendedHeader().value().geoForward() == true);
    REQUIRE(packet.extendedHeader().value().unicast() == true);
    REQUIRE(packet.extendedHeader().value().signature() == false);
}

TEST_CASE("PacketBuilder extendedHeader signature", "[single-file]")
{
    auto packet = PacketParser<100, 100>::parse(makeVector({0x80, 0x12, 0x56, 0x34, 0xB0, 0x98, 0x54, 0x76, 0x32, 0x54, 0x76, 0x98, }));
    REQUIRE(packet.signature().value() == 0x98765432);
}

TEST_CASE("PacketBuilder Tracking ", "[single-file]")
{
    auto packet = PacketParser<100, 100>::parse(makeVector({0x01, 0x12, 0x56, 0x34, 0xC0, 0x0E, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5B, 0x00, 0x00, 0x19 }));
    REQUIRE(packet.header().type() == Header::MessageType::TRACKING);
    TrackingPayload tp = etl::get<TrackingPayload>(packet.payload().value());
    // NOTE deserialisation of complete packet is tested in TrackingPayload tests
    REQUIRE(tp.turnRate() == Catch::Approx(6.2).margin(0.5));
}

TEST_CASE("PacketBuilder Name ", "[single-file]")
{
    auto packet = PacketParser<100, 100>::parse(makeVector({0x02, 0x12, 0x56, 0x34, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64, }));
    REQUIRE(packet.header().type() == Header::MessageType::NAME);
    auto name = etl::get<NamePayload<100>>(packet.payload().value());
    // NOTE deserialisation of complete packet is tested in TrackingPayload tests
    REQUIRE(name.name() == "Hello World" );
}

