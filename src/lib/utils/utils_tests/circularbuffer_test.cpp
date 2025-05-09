
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#define private public

#include <stdio.h>
#include <cstring>
#include "circularbuffer.hpp"

using namespace Catch::Matchers;

TEST_CASE("CircularBuffer with 12 chars", "[single-file]")
{
    char test[128];

    CircularBuffer<16> buffer;
    buffer.push("abcdefghijkl", 12);
    REQUIRE(buffer.available() == 4);

    SECTION("With push to much noop")
    {
        buffer.push("123456", 6);
        REQUIRE(buffer.available() == 4);

        SECTION("Buffer should be intact")
        {
            auto [part, len] = buffer.peek();
            strncpy(test, part, len);
            test[len] = '\0';
            REQUIRE_THAT(test, Equals("abcdefghijkl"));
        }
    }

    SECTION("Accept 4 chars")
    {
        buffer.accepted(4);
        REQUIRE(buffer.available() == 8);

        auto [part, len] = buffer.peek();
        strncpy(test, part, len);
        test[len] = '\0';
        REQUIRE_THAT(test, Equals("efghijkl"));

        SECTION("Accept 4 chars, push 8, until end")
        {
            buffer.push("12345678", 8);
            REQUIRE(buffer.available() == 0);

            auto [part, len] = buffer.peek();
            strncpy(test, part, len);
            test[len] = '\0';
            REQUIRE_THAT(test, Equals("efghijkl1234"));

            SECTION("Accept 4 chars, push 8, until end")
            {
                buffer.accepted(len);
                auto [part, len] = buffer.peek();
                strncpy(test, part, len);
                test[len] = '\0';
                REQUIRE_THAT(test, Equals("5678"));
                buffer.accepted(len);
                REQUIRE(buffer.available() == 16);
            }
        }
    }
}

TEST_CASE("CircularBuffer push full", "[single-file]")
{
    char test[128];

    CircularBuffer<16> buffer;
    buffer.push("abcdefghijklmnop", 16);
    REQUIRE(buffer.available() == 0);

    auto [part, len] = buffer.peek();
    strncpy(test, part, len);
    test[len] = '\0';
    REQUIRE_THAT(test, Equals("abcdefghijklmnop"));

    SECTION("Accept 16, push again")
    {
        buffer.accepted(16);
        buffer.push("abcdefghijklmnop", 16);
        REQUIRE(buffer.available() == 0);

        auto [part, len] = buffer.peek();
        strncpy(test, part, len);
        test[len] = '\0';
        REQUIRE_THAT(test, Equals("abcdefghijklmnop"));
    }
}

TEST_CASE("Wrap Test with get", "[single-file]")
{
    char test[128];
    CircularBuffer<16> buffer;
    buffer.push("aaaaaaaaaaaa", 12);
    REQUIRE(buffer.available() == 4);
    buffer.accepted(4);

    buffer.push("12345678", 8);
    REQUIRE(buffer.available() == 0);

    {
        auto [part, len] = buffer.get();
        strncpy(test, part, len);
        test[len] = '\0';
        REQUIRE_THAT(test, Equals("aaaaaaaa1234"));
    }
    {
        auto [part, len] = buffer.get();
        strncpy(test, part, len);
        test[len] = '\0';
        REQUIRE_THAT(test, Equals("5678"));
    }
    REQUIRE(buffer.available() == 16);
}

TEST_CASE("Wrap Test with peek", "[single-file]")
{
    char test[128];
    CircularBuffer<16> buffer;
    buffer.push("aaaaaaaaaaaa", 12);
    REQUIRE(buffer.available() == 4);
    buffer.accepted(4);

    buffer.push("12345678", 8);
    REQUIRE(buffer.available() == 0);

    {
        auto [part, len] = buffer.peek();
        strncpy(test, part, len);
        test[len] = '\0';
        REQUIRE_THAT(test, Equals("aaaaaaaa1234"));
        buffer.accepted(len);
    }
    {
        auto [part, len] = buffer.peek();
        strncpy(test, part, len);
        test[len] = '\0';
        REQUIRE_THAT(test, Equals("5678"));
        buffer.accepted(len);
    }
    REQUIRE(buffer.available() == 16);
}