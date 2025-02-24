#include <catch2/catch_test_macros.hpp>
#include "../fanet/packetParser.hpp"
#include "helpers.hpp"

using namespace FANET;


// TEST_CASE("PacketBuilder ACK ", "[single-file]")
// {
//     PacketBuilder pb{0x123456};
//     pb.setDestination(0x987654);
//     etl::vector<uint8_t, 8> expected = {0x80, 0x12, 0x56, 0x34, 0x20, 0x98, 0x54, 0x76 };
//     REQUIRE(pb.build(AckPayload{}) == expected);
// }

// TEST_CASE("PacketBuilder ACK signature", "[single-file]")
// {
//     PacketBuilder pb{0x123456};
//     pb.setSignature(0x98765432);
//     pb.setDestination(0x987654);
//     etl::vector<uint8_t, 12> expected = {0x80, 0x12, 0x56, 0x34, 0x30, 0x98, 0x54, 0x76, 0x32, 0x54, 0x76, 0x98};
//     REQUIRE(pb.build(AckPayload{}) == expected);
// }


TEST_CASE("PacketBuilder Tracking Payload", "[single-file]")
{
//     TrackingPayload tp;
//     tp.longitude(52.3);
//     tp.latitude(13.4);
//     tp.speed(45.6);

//     puts("Tracking Payload");
//     PacketBuilder pb{Address(0x12, 0x3456)};
// //    pb.setSignature(0x98765432);
// //    pb.setDestination(Address{0x345678});


//     dumpHex(pb.build(tp));
}

// TEST_CASE("PacketBuilder Service Payload", "[single-file]")
// {
//     ServicePayload sp;

//     puts("Service Payload");
//     PacketBuilder pb{Address{0x123456}};
//     dumpHex(pb.build(sp));
// }
