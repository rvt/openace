#include <catch2/catch_test_macros.hpp>
#include "../fanet/name.hpp"
#include "helpers.hpp"

using namespace FANET;


TEST_CASE("Name name", "[single-file]")
{
    NamePayload<100> payload;
    payload.name("Hello World");
    REQUIRE(payload.name() == "Hello World");
}

TEST_CASE("Name name empty", "[single-file]")
{
    NamePayload<100> payload;
    payload.name("");
    REQUIRE(payload.name() == "");
}

TEST_CASE("Name Serialize/Deserialize ", "[single-file]")
{
    NamePayload<100> payload;
    payload.name("Fanet is nice");

    auto result = createRadioPacket(payload);
    REQUIRE(result == makeVector({0x46, 0x61, 0x6E, 0x65, 0x74, 0x20, 0x69, 0x73, 0x20, 0x6E, 0x69, 0x63, 0x65}));

    auto reader = createReader(result);
    auto received=NamePayload<100>::deserialize(reader);
    REQUIRE(received.name() == "Fanet is nice");
}

TEST_CASE("Name Serialize/Deserialize small Size", "[single-file]")
{
    NamePayload<100> payload;
    payload.name("Fanet is nice");

    auto result = createRadioPacket(payload);
    auto reader = createReader(result);
    auto received=NamePayload<5>::deserialize(reader);
    REQUIRE(received.name() == "Fanet");
}

TEST_CASE("Name Serialize/Deserialize invalid size", "[single-file]")
{
    NamePayload<100> payload;
    etl::vector<uint8_t, 100> empty;
    auto reader = createReader(empty);
    auto received=NamePayload<10>::deserialize(reader);
    REQUIRE(received.name().size() == 0);
}