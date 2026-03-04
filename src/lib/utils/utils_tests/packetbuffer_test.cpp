
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <stdio.h>
#include "../ace/packetbuffer.hpp"
#include "ace/coreutils.hpp"
#include "testhelpers.h"

TEST_CASE("Add packet", "[single-file]")
{
    PacketBuffer<5, 5> buf;
    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4, 0);
    REQUIRE(buf.set(packet1));

    etl::span<uint8_t> data = {};
    REQUIRE(buf.read(data));
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2, 3, 4, 0), data));
}

TEST_CASE("Bigger size, should allow false", "[single-file]")
{
    PacketBuffer<5, 5> buf;
    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4, 5, 6);
    REQUIRE_FALSE(buf.set(packet1));
    REQUIRE(0 == buf.used());
}

TEST_CASE("Exact size, should allow false", "[single-file]")
{
    PacketBuffer<6, 5> buf;
    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4, 5, 6);
    REQUIRE(buf.set(packet1));
    REQUIRE(6 == buf.used());

    etl::span<uint8_t> out = {};
    REQUIRE(buf.read(out));
    REQUIRE(etl::equal(out.begin(), out.end(), packet1.begin()));
}

TEST_CASE("Empty input")
{
    PacketBuffer<5, 5> buf;
    etl::span<uint8_t> data = {};

    REQUIRE(buf.set({}));
    REQUIRE(buf.read(data));
    REQUIRE(data.empty());
    REQUIRE(0 == buf.used());
}

TEST_CASE("StreamBuffer basic append and read", "[StreamBuffer]")
{
    PacketBuffer<64, 4> buf;
    std::vector<uint8_t> packet1 = {1, 2, 3, 4};
    std::vector<uint8_t> packet2 = {5, 6, 7};

    REQUIRE(buf.used() == 0);

    // Add first packet
    REQUIRE(buf.set(packet1));
    REQUIRE(buf.used() == packet1.size());

    // Add second packet
    REQUIRE(buf.set(packet2));
    REQUIRE(buf.used() == packet1.size() + packet2.size());

    // Read first packet
    etl::span<uint8_t> out;
    REQUIRE(buf.read(out, packet1.size()));
    REQUIRE(out.size() == packet1.size());
    REQUIRE(etl::equal(out.begin(), out.end(), packet1.begin()));

    // Read second packet
    REQUIRE(buf.read(out, packet2.size()));
    REQUIRE(out.size() == packet2.size());
    REQUIRE(etl::equal(out.begin(), out.end(), packet2.begin()));

    // No more packets
    REQUIRE_FALSE(buf.read(out));
}

TEST_CASE("StreamBuffer clear", "[StreamBuffer]")
{
    PacketBuffer<32, 2> buf;
    std::vector<uint8_t> data = {10, 20, 30};

    REQUIRE(buf.set(data));
    REQUIRE(buf.used() == data.size());

    buf.clear();
    REQUIRE(buf.used() == 0);

    etl::span<uint8_t> out;
    REQUIRE_FALSE(buf.read(out));
}

TEST_CASE("StreamBuffer overflow handling", "[StreamBuffer]")
{
    PacketBuffer<8, 2> buf;
    std::vector<uint8_t> largePacket = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    REQUIRE_FALSE(buf.set(largePacket));
    REQUIRE(0 == buf.used());
}

TEST_CASE("StreamBuffer overflow with multiple", "[StreamBuffer]")
{
    PacketBuffer<14, 2> buf;
    std::vector<uint8_t> largePacket = {1, 2, 3, 4, 5, 6, 7, 8};
    REQUIRE(buf.set(largePacket));
    REQUIRE_FALSE(buf.set(largePacket));

    etl::span<uint8_t> out;
    REQUIRE(buf.read(out));
    REQUIRE_FALSE(buf.read(out));
}

TEST_CASE("StreamBuffer compacting", "[StreamBuffer]")
{
    PacketBuffer<16, 4> buf;
    std::vector<uint8_t> packet1 = {1, 2, 3};
    std::vector<uint8_t> packet2 = {4, 5, 6};

    REQUIRE(buf.set(packet1));
    REQUIRE(buf.set(packet2));

    // Simulate compact call
    buf.compact();

    REQUIRE(6 == buf.used());

    etl::span<uint8_t> out;
    REQUIRE(buf.read(out, packet1.size()));
    REQUIRE(out.size() == packet1.size());
    buf.compact();
    REQUIRE(3 == buf.used());

    REQUIRE(buf.read(out, packet2.size()));
    REQUIRE(out.size() == packet2.size());
    buf.compact();
    REQUIRE(0 == buf.used());
}

TEST_CASE("StreamBuffer mwhole packets only", "[StreamBuffer]")
{
    PacketBuffer<12, 4> buf;
    std::vector<uint8_t> packet1 = {1, 2};
    std::vector<uint8_t> packet2 = {3, 4, 5};
    std::vector<uint8_t> packet3 = {6, 7, 8, 9};

    REQUIRE(buf.set(packet1));
    REQUIRE(buf.set(packet2));
    REQUIRE(buf.set(packet3));

    etl::span<uint8_t> out;
    REQUIRE(buf.read(out, 4));
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2), out));
    REQUIRE(9 == buf.used());

    REQUIRE(buf.read(out, 8));
    REQUIRE(span_equal(etl::make_array<uint8_t>(3, 4, 5, 6, 7, 8, 9), out));

    REQUIRE(9 == buf.used());
    REQUIRE_FALSE(buf.set(packet3));
    buf.compact();
    REQUIRE(buf.set(packet3));
    REQUIRE(4 == buf.used());
}

TEST_CASE("StreamBuffer should not compact on set", "[StreamBuffer]")
{
    PacketBuffer<8, 4> buf;
    std::vector<uint8_t> packet1 = {1, 2, 3};
    std::vector<uint8_t> packet2 = {4, 5, 6};

    REQUIRE(buf.set(packet1));
    REQUIRE(buf.set(packet2));

    etl::span<uint8_t> out;
    REQUIRE(buf.read(out, packet1.size()));
    REQUIRE(out.size() == packet1.size());

    REQUIRE_FALSE(buf.set(packet2));
}

TEST_CASE("StreamBuffer set with etl::string_view")
{
    PacketBuffer<64, 3> buf;
    etl::string_view sentence1 = "Hello, world!";
    etl::string_view sentence2 = "Test packet";

    // Add first sentence
    REQUIRE(buf.setString(sentence1));

    etl::span<uint8_t> out;
    REQUIRE(buf.read(out));
    std::string result1(out.begin(), out.end());
    REQUIRE(result1 == "Hello, world!");

    // Add second sentence
    REQUIRE(buf.setString(sentence2));

    REQUIRE(buf.read(out));
    std::string result2(out.begin(), out.end());
    REQUIRE(result2 == "Test packet");

    // Buffer should now be empty
    REQUIRE_FALSE(buf.read(out));
}

