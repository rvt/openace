#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../include/adsl/utils.hpp"
#include "../include/adsl/header.hpp"
#include "helpers.hpp"

using namespace ADSL;

TEST_CASE("Header Default Constructor and roundtrip", "[Header]")
{
    Header header;

    REQUIRE(header.type() == Header::PayloadTypeIdentifier::RESERVED1);
    REQUIRE(header.addressMappingTable() == 0x05);
    REQUIRE(header.address() == Address(0));
    REQUIRE(header.forward() == false);

    auto packet = createRadioPacket([&](etl::bit_stream_writer &writer)
                                    { 
        header.serialize(writer);
         REQUIRE( 40 == writer.size_bits() ); });

    auto reader = createReader(packet);
    auto received = Header::deserialize(reader);

    REQUIRE(received.type() == header.type());
    REQUIRE(received.addressMappingTable() == header.addressMappingTable());
    REQUIRE(received.address() == header.address());
    REQUIRE(received.forward() == header.forward());
}

TEST_CASE("Header serialize/deserialize with custom values", "[Header]")
{
    Header header;
    header.type(Header::PayloadTypeIdentifier::TRAFFIC);
    header.addressMappingTable(0x15);
    header.address(Address(0x12, 0x3456));
    header.forward(true);

    auto packet = createRadioPacket([&](etl::bit_stream_writer &writer)
                                    { header.serialize(writer); });

    auto reader = createReader(packet);
    auto received = Header::deserialize(reader);

    REQUIRE(received.type() == Header::PayloadTypeIdentifier::TRAFFIC);
    REQUIRE(received.addressMappingTable() == 0x15);
    REQUIRE(received.address().manufacturer() == 0x12);
    REQUIRE(received.address().unique() == 0x3456);
    REQUIRE(received.forward() == true);
}
