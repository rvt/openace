
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <stdio.h>
#include "streambuffer.hpp"
#include "ace/coreutils.hpp"
#include "mockutils.h"

TEST_CASE("Smaller size, incomplete", "[single-file]")
{
    StreamBuffer<5, 0> buffer;
    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4);
    buffer.set(packet1);

    etl::span<uint8_t> data = {};
    REQUIRE(false == buffer.read(data));
    REQUIRE(data.size() == 0);
}

TEST_CASE("Smaller size, complete", "[single-file]")
{
    StreamBuffer<5, 0> buffer;
    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4, 0);
    buffer.set(packet1);

    etl::span<uint8_t> data = {};
    REQUIRE(true == buffer.read(data));
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2, 3, 4, 0), data));
}

TEST_CASE("Bigger size, incomplete", "[single-file]")
{
    StreamBuffer<5, 0> buffer;
    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4, 5, 6);
    buffer.set(packet1);

    etl::span<uint8_t> data = {};
    REQUIRE(false == buffer.read(data));
    REQUIRE(0 == data.size());
    REQUIRE(0 == buffer.peekBuffer().size());
    REQUIRE(0 == buffer.peekPacket().size());
}

TEST_CASE("Exact Size", "[single-file]")
{
    StreamBuffer<5, 0> buffer;
    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4, 0);
    buffer.set(packet1);

    etl::span<uint8_t> data = {};
    REQUIRE(true == buffer.read(data));
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2, 3, 4, 0), data));
    REQUIRE(false == buffer.read(data));
}

TEST_CASE("Double Separator", "[single-file]")
{
    StreamBuffer<15, 0> buffer;
    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4, 0, 0, 3, 4, 5, 6, 0);
    buffer.set(packet1);

    etl::span<uint8_t> data = {};
    REQUIRE(true == buffer.read(data));
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2, 3, 4, 0), data));
    REQUIRE(true == buffer.read(data));
    REQUIRE(true == buffer.read(data)); // Edge case???
    REQUIRE(span_equal(etl::make_array<uint8_t>(3, 4, 5, 6, 0), data));
}

TEST_CASE("Small packets than big completion", "[single-file]")
{
    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4);
    auto packet2 = etl::make_array<uint8_t>(5, 6, 7);
    auto packet3 = etl::make_array<uint8_t>(0x0A, 0, 0x0B, 1, 0, 0x0C, 6, 7, 8, 9, 8, 0, 0x0D, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2);

    StreamBuffer<9, 0> buffer;
    etl::span<uint8_t> data = {};
    buffer.set(packet1);

    REQUIRE(false == buffer.read(data));
    REQUIRE(0 == data.size());
    buffer.set(packet2);

    REQUIRE(false == buffer.read(data));
    REQUIRE(0 == data.size());
    buffer.set(packet3);
    // printf("- "); CoreUtils::printBuffer(data, true);
    REQUIRE(true == buffer.read(data));
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2, 3, 4, 5, 6, 7, 0x0a, 0), data));

    REQUIRE(true == buffer.read(data));
//    printf("- "); CoreUtils::printBuffer(data, true);
    REQUIRE(span_equal(etl::make_array<uint8_t>(0x0b, 1, 0), data));

    REQUIRE(true == buffer.read(data));
    REQUIRE(span_equal(etl::make_array<uint8_t>(0x0C, 6, 7, 8, 9, 8, 0), data));

    REQUIRE(true == buffer.read(data));
    REQUIRE(span_equal(etl::make_array<uint8_t>(0x0D, 2, 3, 4, 5, 6, 7, 8, 0), data));

    REQUIRE(false == buffer.read(data));

    // CHeck internal buffer
    etl::span<uint8_t> pBuffer = buffer.peekBuffer();
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2), pBuffer));
    REQUIRE(1 == pBuffer.data()[0]);
    REQUIRE(2 == pBuffer.data()[1]);
}

TEST_CASE("Empty input") {
    StreamBuffer<5, 0> buffer;
    etl::span<uint8_t> data = {};
    REQUIRE_FALSE(buffer.set({}));
    REQUIRE_FALSE(buffer.read(data));
    REQUIRE(data.empty());
}

TEST_CASE("Separator at start") {
    StreamBuffer<5, 0> buffer;
    auto pkt = etl::make_array<uint8_t>(0, 1, 2, 3);
    buffer.set(pkt);
    etl::span<uint8_t> data;
    REQUIRE(buffer.read(data));
    REQUIRE(span_equal(etl::make_array<uint8_t>(0), data));
}

TEST_CASE("Separator at end exact fit") {
    StreamBuffer<6, 0> buffer;
    auto pkt = etl::make_array<uint8_t>(1, 2, 3, 4, 5, 0);
    buffer.set(pkt);
    etl::span<uint8_t> data;
    REQUIRE(buffer.read(data));
    REQUIRE(span_equal(pkt, data));
}

TEST_CASE("No separator too big") {
    StreamBuffer<5, 0> buffer;
    auto pkt = etl::make_array<uint8_t>(1, 2, 3, 4, 5, 6);
    REQUIRE_FALSE(buffer.set(pkt));
    REQUIRE(buffer.peekBuffer().empty());
    REQUIRE(buffer.peekPacket().empty());
}

TEST_CASE("Triple message in one packet") {
    StreamBuffer<20, 0> buffer;
    auto pkt = etl::make_array<uint8_t>(1, 0, 2, 0, 3, 0);
    buffer.set(pkt);
    etl::span<uint8_t> data;
    REQUIRE(buffer.read(data));
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 0), data));
    REQUIRE(buffer.read(data));
    REQUIRE(span_equal(etl::make_array<uint8_t>(2, 0), data));
    REQUIRE(buffer.read(data));
    REQUIRE(span_equal(etl::make_array<uint8_t>(3, 0), data));
    REQUIRE_FALSE(buffer.read(data));
}

TEST_CASE("Clear resets state") {
    StreamBuffer<5, 0> buffer;
    buffer.set(etl::make_array<uint8_t>(1, 2, 3, 0));
    etl::span<uint8_t> data;
    buffer.clear();
    REQUIRE_FALSE(buffer.read(data));
    REQUIRE(data.empty());
}

TEST_CASE("Split packet completion") {
    StreamBuffer<6, 0> buffer;
    buffer.set(etl::make_array<uint8_t>(1, 2, 3));
    etl::span<uint8_t> data;
    REQUIRE_FALSE(buffer.read(data));
    buffer.set(etl::make_array<uint8_t>(4, 0));
    REQUIRE(buffer.read(data));
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2, 3, 4, 0), data));
}