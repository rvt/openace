#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "binarymessages.hpp"
#include "coreutils.hpp"
#include "mockconfig.h"
#include "../../gatasconnect/ace/cobsstreamhandler.hpp"
#include "pico/time.h"

namespace
{
    class PressureReceiver : public etl::message_router<PressureReceiver, GATAS::BarometricPressureMsg, GATAS::IngressAircraftPositionsMsg>
    {
        const GATAS::OwnshipState &ownshipState;

    public:
        explicit PressureReceiver(const GATAS::OwnshipState &state) : ownshipState(state) {}

        etl::vector<GATAS::PressureAltQnh, 8> samples;
        size_t trafficBatches = 0;

        void on_receive(const GATAS::BarometricPressureMsg &)
        {
            samples.push_back(ownshipState.pressureAltQnh.load());
        }

        void on_receive(const GATAS::IngressAircraftPositionsMsg &)
        {
            ++trafficBatches;
        }

        void on_receive_unknown(const etl::imessage &)
        {
        }
    };
}

TEST_CASE("Ownship pressure V1 matches the Rust wire fixture", "[binarymessages][pressure]")
{
    // Literal fixture from Rust protocol/tests/cobs_messages.rs, independent of our enum.
    const uint8_t raw[] = {11, 0x04, 0x42, 0x27, 0x95, 1};
    REQUIRE(BinaryMessages::DataType::OWNSHIP_PRESSURE_ALTITUDE_V1 == 11);
    time_us_Value = 12'345'678;
    etl::bit_stream_reader reader(raw, sizeof(raw), etl::endian::big);
    const auto result = BinaryMessages::deserializeOwnshipPressureAltitudeV1(reader);
    REQUIRE(result.has_value());
    REQUIRE(result->pressureAlt == 90);
    REQUIRE(result->qnh == Catch::Approx(1013.3f));
    REQUIRE(result->source == static_cast<GATAS::PressureSource>(1));
    const GATAS::BarometricPressure sensor{987.6f, GATAS::PressureSource::PressureSensor};
    REQUIRE(sensor.pressurehPa == 987.6f);
    REQUIRE(sensor.source == GATAS::PressureSource::PressureSensor);
    const GATAS::PressureAltQnh empty{};
    REQUIRE(empty.pressureAlt == GATAS::INVALID_BARO_ALTITUDE);
    REQUIRE(empty.source == GATAS::PressureSource::Unavailable);
}

TEST_CASE("Ownship pressure V1 preserves unsigned bounds and independent missing fields", "[binarymessages][pressure]")
{
    struct Case
    {
        uint8_t raw[6];
        int32_t altitude;
        float qnh;
    };
    const Case cases[] = {
        {{11, 0, 0, 0x27, 0x10, 0}, -1000, 1000.0f},
        {{11, 0x03, 0x5D, 0x28, 0x3C, 0}, -139, 1030.0f},
        {{11, 0x03, 0xE8, 0xFF, 0xFF, 0}, 0, GATAS::INVALID_QNH},
        {{11, 0xFF, 0xFE, 0x27, 0x95, 0}, 64534, 1013.3f},
        {{11, 0xFF, 0xFF, 0x27, 0x95, 0}, GATAS::INVALID_BARO_ALTITUDE, 1013.3f},
        {{11, 0xFF, 0xFF, 0xFF, 0xFF, 0}, GATAS::INVALID_BARO_ALTITUDE, GATAS::INVALID_QNH},
    };
    for (const auto &test : cases)
    {
        etl::bit_stream_reader reader(test.raw, sizeof(test.raw), etl::endian::big);
        const auto result = BinaryMessages::deserializeOwnshipPressureAltitudeV1(reader);
        REQUIRE(result.has_value());
        REQUIRE(result->pressureAlt == test.altitude);
        REQUIRE(result->qnh == Catch::Approx(test.qnh));
        REQUIRE(result->source == GATAS::PressureSource::Unavailable);
    }
}

TEST_CASE("Ownship pressure V1 rejects wrong types and payload lengths", "[binarymessages][pressure]")
{
    uint8_t raw[] = {11, 0x04, 0x42, 0x27, 0x95, 42};
    for (size_t size = 0; size <= sizeof(raw); ++size)
    {
        if (size == 6)
        {
            continue;
        }
        etl::bit_stream_reader reader(raw, size, etl::endian::big);
        REQUIRE_FALSE(BinaryMessages::deserializeOwnshipPressureAltitudeV1(reader).has_value());
    }
    raw[0] = 1;
    etl::bit_stream_reader reader(raw, 6, etl::endian::big);
    REQUIRE_FALSE(BinaryMessages::deserializeOwnshipPressureAltitudeV1(reader).has_value());
}

TEST_CASE("Ownship pressure source decodes all origins and tolerates future values", "[binarymessages][pressure]")
{
    using Source = GATAS::PressureSource;
    struct Case { uint8_t raw; Source expected; };
    const Case cases[] = {
        {0, Source::Unavailable}, {4, Source::Calculated}, {5, Source::PressureSensor},
        {6, static_cast<Source>(6)}, {255, static_cast<Source>(255)},
    };
    for (const auto &test : cases)
    {
        const uint8_t raw[] = {11, 0x04, 0x42, 0x27, 0x95, test.raw};
        etl::bit_stream_reader reader(raw, sizeof(raw), etl::endian::big);
        const auto decoded = BinaryMessages::deserializeOwnshipPressureAltitudeV1(reader);
        REQUIRE(decoded.has_value());
        REQUIRE(decoded->source == test.expected);
        REQUIRE(decoded->pressureAlt == 90);
    }
}

TEST_CASE("COBS ownship pressure routes split and concatenated frames without traffic", "[binarymessages][pressure]")
{
    CoreUtils::init();
    GATAS::OwnshipState state;
    state.init(CoreUtils::sharedSpinLock());
    state.barometricPressure.store(GATAS::BarometricPressure{987.5f, GATAS::PressureSource::PressureSensor});
    etl::message_bus<2> bus;
    MockConfig config{bus};
    PressureReceiver receiver{state};
    bus.subscribe(receiver);
    CobsStreamHandler handler{bus, config, state};

    // Source-aware frame, followed by a legacy frame without a source byte.
    uint8_t first[] = {7, 11, 0x04};
    handler.handle(0.0f, 0.0f, etl::span<uint8_t>(first, sizeof(first)));
    REQUIRE(receiver.samples.empty());
    REQUIRE(state.pressureAltQnh.load().pressureAlt == GATAS::INVALID_BARO_ALTITUDE);
    uint8_t rest[] = {0x42, 0x27, 0x95, 1, 0, 6, 11, 255, 255, 255, 255, 1, 0};
    handler.handle(0.0f, 0.0f, etl::span<uint8_t>(rest, sizeof(rest)));
    REQUIRE(receiver.samples.size() == 2);
    REQUIRE(receiver.samples[0].pressureAlt == 90);
    REQUIRE(receiver.samples[0].qnh == Catch::Approx(1013.3f));
    REQUIRE(receiver.samples[0].source == static_cast<GATAS::PressureSource>(1));
    REQUIRE(receiver.samples[1].source == GATAS::PressureSource::Unavailable);
    REQUIRE(receiver.samples[1].pressureAlt == GATAS::INVALID_BARO_ALTITUDE);
    REQUIRE(receiver.samples[1].qnh == GATAS::INVALID_QNH);
    REQUIRE(state.pressureAltQnh.load().pressureAlt == GATAS::INVALID_BARO_ALTITUDE);
    REQUIRE(state.pressureAltQnh.load().qnh == GATAS::INVALID_QNH);
    REQUIRE(state.pressureAltQnh.load().source == GATAS::PressureSource::Unavailable);
    REQUIRE(receiver.trafficBatches == 0);

    // Invalid COBS, then valid COBS with a truncated payload, then a valid frame.
    uint8_t recovery[] = {8, 11, 1, 0, 5, 11, 0x04, 0x42, 0x27, 0, 7, 11, 0x04, 0x42, 0x27, 0x95, 1, 0};
    handler.handle(0.0f, 0.0f, etl::span<uint8_t>(recovery, sizeof(recovery)));
    REQUIRE(receiver.samples.size() == 3);
    REQUIRE(receiver.samples.back().pressureAlt == 90);
    REQUIRE(state.pressureAltQnh.load().pressureAlt == 90);
    REQUIRE(state.pressureAltQnh.load().qnh == Catch::Approx(1013.3f));
    REQUIRE(receiver.trafficBatches == 0);

    uint8_t malformed[] = {8, 11, 1, 0, 5, 11, 0x04, 0x42, 0x27, 0};
    handler.handle(0.0f, 0.0f, etl::span<uint8_t>(malformed, sizeof(malformed)));
    REQUIRE(receiver.samples.size() == 3);
    REQUIRE(state.pressureAltQnh.load().pressureAlt == 90);
    REQUIRE(state.pressureAltQnh.load().qnh == Catch::Approx(1013.3f));
    bus.unsubscribe(receiver);
}
