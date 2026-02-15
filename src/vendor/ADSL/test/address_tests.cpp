
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../include/adsl/utils.hpp"
#include "../include/adsl/address.hpp"
#include "helpers.hpp"

using namespace ADSL;

TEST_CASE("Address Test", "[single-file]")
{
  SECTION("Basic cases")
  {
    ADSL::Address addr1(0x345612);
    REQUIRE(addr1.manufacturer() == 0x12);
    REQUIRE(addr1.unique() == 0x3456);
    REQUIRE(addr1.asUint() == 0x345612);
  }

  SECTION("Serialisation")
  {
    ADSL::Address addr1(0xAB, 0xCDEF);
    auto packet = createRadioPacket([&](etl::bit_stream_writer &writer)
                                { addr1.serialize(writer); 
                                REQUIRE( 24 == writer.size_bits() );});

    REQUIRE(packet == makeVector({0xD5, 0xF7, 0xB3}));
  }

  SECTION("Deserialization from real packet")
  {
    // Stream taken from a real packet with address 0x234567
    auto buffer = etl::make_array<uint8_t>(0x00, 0x00, 0xC0, 0x59, 0xD1, 0x08, 0x66);
    for (size_t i=0; i< buffer.size(); ++i)
    {
      buffer[i] = etl::reverse_bits(buffer[i]);
    }

    etl::bit_stream_reader reader(buffer.data(), buffer.size(), etl::endian::little);
    reader.read_unchecked<uint8_t>(8);  // Header
    reader.read_unchecked<uint8_t>(8);  // Payload Ident
    reader.read_unchecked<uint16_t>(6); // Address Mapping
    ADSL::Address addr2 = ADSL::Address::deserialize(reader);

    printf("Manufacturer: 0x%02X, Unique: 0x%04X, AsUint: 0x%06X\n",
           addr2.manufacturer(), addr2.unique(), addr2.asUint());


    REQUIRE(addr2.asUint() == 0x234567);
    REQUIRE(addr2.manufacturer() == 0x67);
    REQUIRE(addr2.unique() == 0x2345);
  }

  SECTION("Equality/Inequality")
  {
    ADSL::Address addr1(0x12, 0x3456);
    ADSL::Address addr2(0x12, 0x3456);
    ADSL::Address addr3(0x34, 0x5678);

    REQUIRE(addr1 == addr2);
    REQUIRE(addr1 != addr3);
  }
}