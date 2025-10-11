#include <stdio.h>
#include <stdint.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gulp.hpp"
#include "mockutils.h"

TEST_CASE("Gulp: multiple packets", "[Gulp]")
{
    etl::vector<uint8_t, 7> partialBuffer;
    Gulp gulp(partialBuffer, DelimiterBitmap::Null());

    auto packet1 = etl::make_array<uint8_t>(1, 2, 3);
    auto packet2 = etl::make_array<uint8_t>(4, 5, 6, 0);
    auto packet3 = etl::make_array<uint8_t>(8, 9, 10, 11);
    auto packet4 = etl::make_array<uint8_t>(12, 13, 14, 0);

    etl::span<uint8_t> pkt;

    gulp.setRef(packet1);
    REQUIRE_FALSE(gulp.pop_into(pkt));
    REQUIRE(0 == pkt.size());
    pkt={};

    gulp.setRef(packet2);
    REQUIRE(gulp.pop_into(pkt));
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2, 3, 4, 5, 6), pkt));
    pkt={};

    gulp.setRef(packet3);
    REQUIRE_FALSE(gulp.pop_into(pkt));
    REQUIRE(0 == pkt.size());
    pkt={};

    gulp.setRef(packet4);
    REQUIRE(gulp.pop_into(pkt));
    REQUIRE(span_equal(etl::make_array<uint8_t>(8, 9, 10, 11, 12, 13, 14), pkt));
}

TEST_CASE("Gulp: small buffer, large packet packets", "[Gulp]")
{
    etl::vector<uint8_t, 4> partialBuffer;
    Gulp gulp(partialBuffer, DelimiterBitmap::Null());

    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4, 5, 6, 0);
    etl::span<uint8_t> pkt;

    gulp.setRef(packet1);
    REQUIRE(gulp.pop_into(pkt));
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2, 3, 4, 5, 6), pkt));
}

TEST_CASE("Gulp: Push 0 byte, setRef", "[Gulp]")
{
    etl::vector<uint8_t, 8> partialBuffer;
    Gulp gulp(partialBuffer, DelimiterBitmap::Null());

    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4, 5, 6);
    auto packet2 = etl::make_array<uint8_t>(0);
    etl::span<uint8_t> pkt;
    gulp.setRef(packet1);
    REQUIRE_FALSE(gulp.pop_into(pkt)); // Must call pop_into to ensure data is moved into internal buffer

    gulp.setRef(packet2);
    REQUIRE(gulp.pop_into(pkt));

    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2, 3, 4, 5, 6), pkt));
}

TEST_CASE("Gulp: Push 0 byte, pushAndSetRef", "[Gulp]")
{
    etl::vector<uint8_t, 18> partialBuffer;
    Gulp gulp(partialBuffer, DelimiterBitmap::Null());

    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4, 5, 6);
    auto packet2 = etl::make_array<uint8_t>(0, 8, 9, 10, 0);
    etl::span<uint8_t> pkt;
    gulp.pushAndSetRef(packet1);
    gulp.pushAndSetRef(packet2);
    REQUIRE(gulp.pop_into(pkt));
    print_buffer_hex(pkt.data(), pkt.size());
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2, 3, 4, 5, 6), pkt));
    REQUIRE(gulp.pop_into(pkt));
    REQUIRE(span_equal(etl::make_array<uint8_t>(8, 9, 10), pkt));
}

TEST_CASE("Gulp: Push 0 bytes, should pop", "[Gulp]")
{
    etl::vector<uint8_t, 8> partialBuffer;
    Gulp gulp(partialBuffer, DelimiterBitmap::Null());

    auto packet1 = etl::make_array<uint8_t>(0, 0, 0);
    etl::span<uint8_t> pkt;
    gulp.pushAndSetRef(packet1);
    REQUIRE(gulp.pop_into(pkt));
    REQUIRE(pkt.empty());

    REQUIRE(gulp.pop_into(pkt));
    REQUIRE(pkt.empty());

    REQUIRE(gulp.pop_into(pkt));
    REQUIRE(pkt.empty());

    auto packet2 = etl::make_array<uint8_t>(1, 2, 0);
    gulp.pushAndSetRef(packet2);
    REQUIRE(gulp.pop_into(pkt));
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2), pkt));
}

TEST_CASE("Gulp: internal buffer full", "[Gulp]")
{
    etl::vector<uint8_t, 4> partialBuffer;
    Gulp gulp(partialBuffer, DelimiterBitmap::Null());

    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4);
    auto packet2 = etl::make_array<uint8_t>(5, 0, 1, 2, 0);
    etl::span<uint8_t> pkt;

    REQUIRE(gulp.pushAndSetRef(packet1));
    REQUIRE(gulp.pushAndSetRef(packet2));

    gulp.pop_into(pkt); // 4
    print_buffer_hex(pkt.data(), pkt.size());
    REQUIRE(span_equal(etl::make_array<uint8_t>(), pkt));

    gulp.pop_into(pkt); // 4
    print_buffer_hex(pkt.data(), pkt.size());
    REQUIRE(span_equal(etl::make_array<uint8_t>(1, 2), pkt));

    gulp.pop_into(pkt); // 4
    print_buffer_hex(pkt.data(), pkt.size());
    REQUIRE(span_equal(etl::make_array<uint8_t>(), pkt));
}

TEST_CASE("Gulp: small buffer, large packet in two", "[Gulp]")
{
    etl::vector<uint8_t, 4> partialBuffer;
    Gulp gulp(partialBuffer, DelimiterBitmap::Null());

    auto packet1 = etl::make_array<uint8_t>(1, 2, 3, 4);
    auto packet2 = etl::make_array<uint8_t>(5, 0, 1, 2, 0);
    etl::span<uint8_t> pkt;

    REQUIRE(gulp.pushAndSetRef(packet1));
    REQUIRE(gulp.pushAndSetRef(packet2));

    REQUIRE(gulp.pop_into(pkt));
    REQUIRE(span_equal(etl::make_array<uint8_t>(), pkt));
}

TEST_CASE("Gulp: Small buffer pushAndSetRef", "[Gulp]")
{
    etl::vector<uint8_t, 4> partialBuffer;
    Gulp gulp(partialBuffer, DelimiterBitmap::Null());

    auto packet1 = etl::make_array<uint8_t>(1, 0, 3, 4, 0);
    etl::span<uint8_t> pkt;

    REQUIRE(gulp.pushAndSetRef(packet1));

    REQUIRE(gulp.pop_into(pkt));
    print_buffer_hex(pkt.data(), pkt.size());
    REQUIRE(span_equal(etl::make_array<uint8_t>(1), pkt));

    REQUIRE(gulp.pop_into(pkt));
    print_buffer_hex(pkt.data(), pkt.size());
    REQUIRE(span_equal(etl::make_array<uint8_t>(3, 4), pkt));
}

TEST_CASE("Gulp: Small buffer setRef", "[Gulp]")
{
    etl::vector<uint8_t, 4> partialBuffer;
    Gulp gulp(partialBuffer, DelimiterBitmap::Null());

    auto packet1 = etl::make_array<uint8_t>(1, 0, 3, 4, 0);
    etl::span<uint8_t> pkt;

    gulp.setRef(packet1);

    REQUIRE(gulp.pop_into(pkt));
    print_buffer_hex(pkt.data(), pkt.size());
    REQUIRE(span_equal(etl::make_array<uint8_t>(1), pkt));

    REQUIRE(gulp.pop_into(pkt));
    print_buffer_hex(pkt.data(), pkt.size());
    REQUIRE(span_equal(etl::make_array<uint8_t>(3, 4), pkt));
}