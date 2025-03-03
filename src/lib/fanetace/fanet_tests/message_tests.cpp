#include <catch2/catch_test_macros.hpp>
#include "../fanet/message.hpp"
#include "helpers.hpp"

using namespace FANET;


TEST_CASE("Message", "[single-file]")
{
    MessagePayload<100> payload;
    payload.subHeader(0x12);
    REQUIRE(payload.subHeader() == 0x12);
}

TEST_CASE("Message message assign", "[single-file]")
{
    MessagePayload<100> payload;
    payload.message({0x01, 0x02, 0x03, 0x04});
    REQUIRE(payload.message() == makeVector({0x01, 0x02, 0x03, 0x04}));
}

TEST_CASE("Message Serialize/Deserialize ", "[single-file]")
{
    MessagePayload<100> payload;
    payload.subHeader(0x12);
    payload.message({0x01, 0x02, 0x03, 0x04, 0x18});

    auto result = createRadioPacket(payload);
    REQUIRE(result == makeVector({0x12, 0x01, 0x02, 0x03, 0x04, 0x18}));

    auto reader = createReader(result);
    auto received=MessagePayload<100>::deserialize(reader);
    REQUIRE(received.subHeader() == 0x12);
    REQUIRE(received.message() == makeVector({0x01, 0x02, 0x03, 0x04, 0x18}));
}

TEST_CASE("Message Serialize/Deserialize small MessagePayload size ", "[single-file]")
{
    MessagePayload<100> payload;
    payload.message({0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C});
    auto result = createRadioPacket(payload);

    auto reader = createReader(result);
    auto received=MessagePayload<10>::deserialize(reader);
    REQUIRE(received.message() == makeVector({0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A}));
}

TEST_CASE("Message Serialize/Deserialize 0 size", "[single-file]")
{
    etl::vector<uint8_t, 10> buffer;
    auto reader = createReader(buffer);
    auto received=MessagePayload<10>::deserialize(reader);
    REQUIRE(received.message().size() == 0);
    REQUIRE(received.subHeader() == 0x00);
}

TEST_CASE("Message Serialize/Deserialize 1 size", "[single-file]")
{
    auto reader = createReader(makeVector({0x041}));
    auto received=MessagePayload<10>::deserialize(reader);
    REQUIRE(received.message().size() == 0);
    REQUIRE(received.subHeader() == 0x41);
}
