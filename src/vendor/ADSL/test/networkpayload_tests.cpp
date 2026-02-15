
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../include/adsl/networkpayload.hpp"
#include "etl/vector.h"
#include "helpers.hpp"

using namespace ADSL;

// ============================================================================
// Default Constructor Tests
// ============================================================================

TEST_CASE("NetworkPayload Default Constructor", "[NetworkPayload]")
{
  NetworkPayload layer;

  REQUIRE(layer.protocolVersion() == NetworkPayload::ProtocolVersion::ISSUE_2);
  REQUIRE(layer.secureSignatureFlag() == false);
  REQUIRE(layer.keyIndex() == 0);
  REQUIRE(layer.errorControlMode() == NetworkPayload::ErrorControlMode::CRC); // Default CRC (0)
}

// ============================================================================
// Key Index Tests (2 bits)
// ============================================================================

TEST_CASE("NetworkPayload Key Index", "[NetworkPayload]")
{
  NetworkPayload layer;

  layer.keyIndex(0);
  REQUIRE(layer.keyIndex() == 0);

  layer.keyIndex(3);
  REQUIRE(layer.keyIndex() == 3);

  // Test clamping > 3
  layer.keyIndex(4);
  REQUIRE(layer.keyIndex() == 3);
}

TEST_CASE("NetworkPayload asUint8/fromUint8", "[NetworkPayload]")
{
  NetworkPayload layer;
  layer.keyIndex(1);
  layer.protocolVersion(NetworkPayload::ProtocolVersion::ISSUE_2);
  layer.secureSignatureFlag(true);
  layer.errorControlMode(NetworkPayload::ErrorControlMode::FEC);

  uint8_t value = layer.asUint8();

  auto from = NetworkPayload::fromUint8(value);
  REQUIRE(1 == from.keyIndex());
  REQUIRE(NetworkPayload::ProtocolVersion::ISSUE_2 == from.protocolVersion());
  REQUIRE(true == from.secureSignatureFlag());
  REQUIRE(true == from.secureSignatureFlag());
  REQUIRE(NetworkPayload::ErrorControlMode::FEC == from.errorControlMode());
}

// ============================================================================
// Error Control Mode Tests (1 bit)
// ============================================================================

TEST_CASE("NetworkPayload Error Control Mode", "[NetworkPayload]")
{
  NetworkPayload layer;

  // Default is false (CRC)
  REQUIRE(layer.errorControlMode() == NetworkPayload::ErrorControlMode::CRC);

  // Set to FEC
  layer.errorControlMode(NetworkPayload::ErrorControlMode::FEC);
  REQUIRE(layer.errorControlMode() == NetworkPayload::ErrorControlMode::FEC);

  // Set to CRC
  layer.errorControlMode(NetworkPayload::ErrorControlMode::CRC);
  REQUIRE(layer.errorControlMode() == NetworkPayload::ErrorControlMode::CRC);
}

// ============================================================================
// Serialization/Deserialization Tests
// ============================================================================

TEST_CASE("NetworkPayload Serialize/Deserialize Basic", "[NetworkPayload]")
{
  NetworkPayload layer;
  layer.keyIndex(2);
  layer.errorControlMode(NetworkPayload::ErrorControlMode::FEC);

  auto packet = createRadioPacket([&](etl::bit_stream_writer &writer)
                                  { layer.serialize_issue1(writer);
                                    REQUIRE( 8 == writer.size_bits() ); });

  REQUIRE(packet == makeVector({0x83}));
  //  REQUIRE(  layer.asUint8() == 0x83 );

  auto reader = createReader(packet);
  auto received = NetworkPayload::deserialize_issue1(reader);

  REQUIRE(received.keyIndex() == 2);
  REQUIRE(received.errorControlMode() == NetworkPayload::ErrorControlMode::FEC);
  REQUIRE(received.protocolVersion() == NetworkPayload::ProtocolVersion::ISSUE_2);
}
