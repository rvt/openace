#include <catch2/catch_test_macros.hpp>

#include "../include/adsl/errorCorrect.hpp"
#include <array>

using namespace ADSL;

namespace
{
    constexpr size_t PacketBytes = 24;
    constexpr std::array<uint32_t, 6> ValidPacketWords{
        0xB229CC00,
        0x9981C4E4,
        0x5DD2E995,
        0x20492D0B,
        0xBD66A043,
        0xEE82CD94,
    };

    template <size_t N>
    etl::span<uint8_t> asBytes(std::array<uint32_t, N> &words)
    {
        return etl::span<uint8_t>(reinterpret_cast<uint8_t *>(words.data()), words.size() * sizeof(uint32_t));
    }

    template <size_t N>
    etl::span<const uint8_t> asBytes(const std::array<uint32_t, N> &words)
    {
        return etl::span<const uint8_t>(reinterpret_cast<const uint8_t *>(words.data()), words.size() * sizeof(uint32_t));
    }

    template <size_t N>
    etl::span<const uint8_t> asConstBytes(const std::array<uint8_t, N> &bytes)
    {
        return etl::span<const uint8_t>(bytes.data(), bytes.size());
    }
}

TEST_CASE("Correct returns 0 for an already valid 24-byte packet", "[errorCorrect]")
{
    auto packet = ValidPacketWords;
    std::array<uint8_t, PacketBytes> err{};

    REQUIRE(Correct(asBytes(packet), asConstBytes(err)) == 0);
    REQUIRE(packet == ValidPacketWords);
}

TEST_CASE("Correct repairs a single flipped bit when the error mask points to it", "[errorCorrect]")
{
    auto packet = ValidPacketWords;
    std::array<uint8_t, PacketBytes> err{};

    FlipBit(asBytes(packet), 160);
    FlipBit(etl::span<uint8_t>(err.data(), err.size()), 160);
    FlipBit(asBytes(packet), 13);
    FlipBit(etl::span<uint8_t>(err.data(), err.size()), 13);

    REQUIRE(Correct(asBytes(packet), asConstBytes(err)) == 2);
    REQUIRE(packet == ValidPacketWords);
}

TEST_CASE("Correct returns -1 when the error mask cannot explain the corruption", "[errorCorrect]")
{
    auto packet = ValidPacketWords;
    std::array<uint8_t, PacketBytes> err{};

    FlipBit(asBytes(packet), 3);
    FlipBit(asBytes(packet), 41);
    FlipBit(asBytes(packet), 87);

    REQUIRE(Correct(asBytes(packet), asConstBytes(err)) == -1);
}
