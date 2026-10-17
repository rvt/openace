#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include "ace/deviceactioncodec.hpp"
#include "../../gatasconnect/ace/cobsstreamhandler.hpp"

using namespace GATAS;

TEST_CASE("Device action wire vectors match Companion", "[device-actions]")
{
    const uint8_t raw[] = {12, 1, 0, 0x12, 0x34, 0x56, 0x78, 1, 2, 3, 4, 5, 6, 7, 8, 0};
    const auto request = DeviceActionCodec::decode(raw);
    REQUIRE(request);
    REQUIRE(request->requestId == 0x12345678);
    REQUIRE(request->bootId == 0x0102030405060708ULL);
    REQUIRE(request->action == DeviceAction::Save);
    REQUIRE_FALSE(DeviceActionCodec::decode(etl::span<const uint8_t>(raw, 15)));
    const uint8_t expected[] = {13, 1, 0x12, 0x34, 0x56, 0x78, 0, 7, 1, 2, 3, 4, 5, 6, 7, 8, 1};
    uint8_t framed[32];
    uint8_t decoded[32];
    const auto written = DeviceActionCodec::encode({DeviceAction::Save, 0x12345678, DeviceActionResult::Ok, 7, 0x0102030405060708ULL, true}, framed, sizeof(framed));
    REQUIRE(decodeCOBS(framed, written, decoded, sizeof(decoded)) == sizeof(expected));
    REQUIRE(std::memcmp(expected, decoded, sizeof(expected)) == 0);
}

TEST_CASE("Prepare never executes and identical permits execute exactly once", "[device-actions]")
{
    DeviceActionCoordinator coordinator;
    int executions = 0;
    auto execute = [&](const DeviceActionRequest &) { ++executions; return DeviceActionResult::Ok; };
    DeviceActionRequest request{DeviceAction::Save, DeviceActionPhase::Prepare, 10, 42, DeviceUpdatePolicy::RequireSaved};
    REQUIRE(coordinator.handle(request, 1, 100, 42, execute) == DeviceActionResult::Accepted);
    REQUIRE(coordinator.handle(request, 1, 200, 42, execute) == DeviceActionResult::Accepted);
    REQUIRE(executions == 0);
    auto conflict = request;
    conflict.action = DeviceAction::Restart;
    REQUIRE(coordinator.handle(conflict, 1, 200, 42, execute) == DeviceActionResult::Invalid);
    request.phase = DeviceActionPhase::Execute;
    REQUIRE(coordinator.handle(request, 1, 300, 42, execute) == DeviceActionResult::Ok);
    REQUIRE(coordinator.handle(request, 1, 400, 42, execute) == DeviceActionResult::Ok);
    REQUIRE(executions == 1);
}

TEST_CASE("Expired disconnected and stale boot permits never execute", "[device-actions]")
{
    DeviceActionCoordinator coordinator;
    int executions = 0;
    auto execute = [&](const DeviceActionRequest &) { ++executions; return DeviceActionResult::Ok; };
    DeviceActionRequest request{DeviceAction::Restart, DeviceActionPhase::Prepare, 10, 42, DeviceUpdatePolicy::RequireSaved};
    REQUIRE(coordinator.handle(request, 0, 0, 42, execute) == DeviceActionResult::Invalid);
    REQUIRE(coordinator.handle(request, 1, 0, 43, execute) == DeviceActionResult::StaleBoot);
    REQUIRE(coordinator.handle(request, 1, 0, 42, execute) == DeviceActionResult::Accepted);
    request.phase = DeviceActionPhase::Execute;
    REQUIRE(coordinator.handle(request, 1, 10'000, 42, execute) == DeviceActionResult::Expired);
    request.requestId = 11;
    request.phase = DeviceActionPhase::Prepare;
    REQUIRE(coordinator.handle(request, 1, 10'001, 42, execute) == DeviceActionResult::Accepted);
    coordinator.disconnect(1);
    request.phase = DeviceActionPhase::Execute;
    REQUIRE(coordinator.handle(request, 1, 10'002, 42, execute) == DeviceActionResult::Expired);
    REQUIRE(coordinator.handle(request, 2, 10'002, 42, execute) == DeviceActionResult::Invalid);
    REQUIRE(executions == 0);
}

TEST_CASE("Management validates policies and serializes preparation across sessions", "[device-actions]")
{
    DeviceActionCoordinator coordinator;
    int executions = 0;
    auto execute = [&](const DeviceActionRequest &) { ++executions; return DeviceActionResult::Ok; };
    DeviceActionRequest first{DeviceAction::Save, DeviceActionPhase::Prepare, 1, 42, DeviceUpdatePolicy::RequireSaved};
    auto invalid = first;
    invalid.updatePolicy = DeviceUpdatePolicy::AllowUnsaved;
    REQUIRE(coordinator.handle(invalid, 9, 0, 42, execute) == DeviceActionResult::Invalid);
    invalid = first;
    invalid.phase = static_cast<DeviceActionPhase>(2);
    REQUIRE(coordinator.handle(invalid, 9, 0, 42, execute) == DeviceActionResult::Invalid);
    invalid = first;
    invalid.requestId = 0;
    REQUIRE(coordinator.handle(invalid, 9, 0, 42, execute) == DeviceActionResult::Invalid);
    REQUIRE(coordinator.handle(first, 9, 0, 42, execute) == DeviceActionResult::Accepted);
    auto second = first;
    second.requestId = 2;
    REQUIRE(coordinator.handle(second, 10, 0, 42, execute) == DeviceActionResult::Busy);
    second = first;
    second.phase = DeviceActionPhase::Execute;
    REQUIRE(coordinator.handle(second, 10, 1, 42, execute) == DeviceActionResult::Invalid);
    REQUIRE(executions == 0);
    coordinator.disconnect(9);
    second = first;
    second.requestId = 2;
    REQUIRE(coordinator.handle(second, 10, 2, 42, execute) == DeviceActionResult::Accepted);
    second.phase = DeviceActionPhase::Execute;
    REQUIRE(coordinator.handle(second, 10, 3, 42, execute) == DeviceActionResult::Ok);
    REQUIRE(executions == 1);
    // Fill the bounded result cache and verify a recent duplicate stays inert.
    for (uint32_t id = 3; id <= 20; ++id)
    {
        second.requestId = id;
        second.phase = DeviceActionPhase::Prepare;
        REQUIRE(coordinator.handle(second, 10, id, 42, execute) == DeviceActionResult::Accepted);
        second.phase = DeviceActionPhase::Execute;
        REQUIRE(coordinator.handle(second, 10, id, 42, execute) == DeviceActionResult::Ok);
    }
    REQUIRE(coordinator.handle(second, 10, 21, 42, execute) == DeviceActionResult::Ok);
    REQUIRE(executions == 19);
}

class ControlReceiver : public etl::message_router<ControlReceiver, DeviceActionRequestMsg>
{
public:
    int count = 0;
    void on_receive(const DeviceActionRequestMsg &msg) { REQUIRE(msg.session == 9); ++count; }
    void on_receive_unknown(const etl::imessage &) {}
};

TEST_CASE("Management bypasses holdback but cannot arrive through UDP", "[device-actions]")
{
    etl::message_bus<2> bus;
    ControlReceiver receiver;
    bus.subscribe(receiver);
    OwnshipState state;
    CobsStreamHandler handler(bus, state);
    const uint8_t raw[] = {12, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 42, 0};
    uint8_t frame[32];
    const auto length = encodeCOBS(raw, sizeof(raw), frame, sizeof(frame), true);
    handler.handle(0.0f, 0.0f, etl::span<uint8_t>(frame, length), 0, false);
    REQUIRE(receiver.count == 0);
    encodeCOBS(raw, sizeof(raw), frame, sizeof(frame), true);
    handler.handle(0.0f, 0.0f, etl::span<uint8_t>(frame, length), 9, false);
    REQUIRE(receiver.count == 1);
}

TEST_CASE("Management frame fragments cannot cross BLE sessions or UDP", "[device-actions]")
{
    etl::message_bus<2> bus;
    ControlReceiver receiver;
    bus.subscribe(receiver);
    OwnshipState state;
    CobsStreamHandler handler(bus, state);
    const uint8_t raw[] = {12, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 42, 0};
    uint8_t frame[32];
    const auto length = encodeCOBS(raw, sizeof(raw), frame, sizeof(frame), true);
    const auto split = length / 2;
    handler.handle(0.0f, 0.0f, etl::span<uint8_t>(frame, split), 8, false);
    handler.handle(0.0f, 0.0f, etl::span<uint8_t>(frame + split, length - split), 9, false);
    REQUIRE(receiver.count == 0);
    encodeCOBS(raw, sizeof(raw), frame, sizeof(frame), true);
    handler.handle(0.0f, 0.0f, etl::span<uint8_t>(frame, split), 0, false);
    handler.handle(0.0f, 0.0f, etl::span<uint8_t>(frame + split, length - split), 9, false);
    REQUIRE(receiver.count == 0);
    encodeCOBS(raw, sizeof(raw), frame, sizeof(frame), true);
    handler.handle(0.0f, 0.0f, etl::span<uint8_t>(frame, length), 9, false);
    REQUIRE(receiver.count == 1);
}
