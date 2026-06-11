# AGENTS.md

This file provides guidance to Codex and other coding agents working in this repository.

## Project Overview

OpenAce is a multi-protocol aviation conspicuity device (OGN, FLARM, ADS-L, FANET, ADS-B) running on Raspberry Pi RP2040/RP2350 with FreeRTOS. It uses a modular message-bus architecture built on the Embedded Template Library (ETL).

## Working Rules

- Prefer targeted edits. Do not touch `src/vendor/` unless the task explicitly requires it.
- Keep desktop test code and embedded code paths aligned when changing shared module behavior.
- Use fixed-size ETL containers and avoid introducing dynamic-allocation-heavy patterns into firmware code.
- Always use braces for control-flow bodies, even when the body is a single line.
- Handle mutexes with RAII `SemaphoreGuard` scopes. Preferred pattern:
  ```cpp
  if (auto guard = SemaphoreGuard(1000, instance->mutex))
  {
      buffer.read(data, ctx.mtu);
  }
  ```
  After the guarded block, RAII releases the mutex. Do not replace this with manual `xSemaphoreTake` / `xSemaphoreGive` pairs unless the task explicitly requires it.
- When changing message routes or module interactions, update [doc/message-bus.md](/Volumes/ext/source/OpenAce/doc/message-bus.md) if the documented flow changed.

## Build Commands

### Unit Tests (Desktop/x86)

```bash
cd src
cmake -B build_test -G Ninja
ninja -C build_test
```

### Running a Single Test Module

Each tested module has a local `urun.sh` script in its `*_tests/` directory. This is the fastest way to iterate:

```bash
cd src/lib/<module>/<module>_tests
./urun.sh
```

Examples that exist today include:

- `src/lib/core/core_tests/urun.sh`
- `src/lib/radiotuner/radiotuner_tests/urun.sh`
- `src/lib/aircrafttracker/aircrafttracker_tests/urun.sh`
- `src/lib/config/config_tests/urun.sh`

You can also run the built test binary directly from the top-level test build:

```bash
cd src
cmake -B build_test -G Ninja
ninja -C build_test
./build_test/lib/<module>/<module>_tests/<target>
```

### Firmware Build (Embedded)

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
export FREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel

cd src/pico
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPICO_PLATFORM=rp2040 -DPICO_BOARD=pico_w
ninja -C build
```

Supported variants:

- Platforms: `rp2040`, `rp2350`
- Boards: `pico_w`, `pico2_w`

The firmware output is a `.uf2` image.

### SystemGUI (Web UI)

The web UI lives in `src/SystemGUI/`.

```bash
cd src/SystemGUI
npm install
npm start
```

For production assets:

```bash
cd src/SystemGUI
npm run build
```

The Parcel build output in `src/SystemGUI/dist/` is embedded into the firmware during the CMake firmware build.

Before running the dev server against hardware, set the device IP in `src/SystemGUI/.proxyrc.js`.

## Architecture

### ACE Module Pattern

Each functional unit is a module that typically inherits from `BaseModule` and an ETL message router:

```cpp
class MyModule : public BaseModule,
                 public etl::message_router<MyModule, Msg1, Msg2, ...>
{
    OpenAce::PostConstruct postConstruct(OpenAce::PostConstruct) override;
    void start() override;
    void on_receive(const Msg1 &msg);
    void on_receive(const Msg2 &msg);
    void on_receive_unknown(const etl::imessage &msg) override {}
};
```

Key base pieces:

- `src/lib/core/ace/basemodule.hpp`: lifecycle management, config access, module registry
- `src/lib/core/ace/messages.hpp`: shared message definitions

### Module Lifecycle

1. Constructor receives `etl::imessage_bus&` and `const Configuration&`
2. `postConstruct()` validates hardware and configuration and returns `PostConstruct::OK` or an error enum
3. `start()` begins operation
4. `getData(...)` and `setData(...)` expose runtime inspection and control

### Message Bus Communication

Modules communicate asynchronously through the ETL message bus. Prefer message-based communication over direct module-to-module calls.

- Define messages in `src/lib/core/ace/messages.hpp`
- Send with `messageBus.receive(FooMsg{...})`
- Receive by listing the message in the router template parameters and implementing `on_receive(const FooMsg&)`

Common message groups include:

- Position: `OwnshipPositionMsg`, `AircraftPositionMsg`
- Radio: `RadioRxManchesterMsg`, `RadioTxPositionRequestMsg`
- Sensors: `GPSSentenceMsg`, `BarometricPressureMsg`
- Timers: `Every5SecMsg`, `Every30SecMsg`

### Configuration

Modules read config through the `Configuration` module:

```cpp
auto val = config.valueByPath(defaultValue, "SectionName", "keyName");
auto str = config.strValueByPath(defaultValue, "SectionName", "keyName");
bool en = config.isModuleEnabled("ModuleName");
```

Primary runtime config file:

- `src/pico/gatas_default_config.ini`

When adding or wiring a new module, update the `[Modules]` section accordingly.

### Hardware Abstraction Base Classes

- `Radio`: radio transceivers, registered as `_Radio_0` through `_Radio_3`
- `SpiModule`: SPI interface, `NAME = "_SPI"`
- `RtcModule`: RTC abstraction
- `BinaryReceiver`: binary stream consumer base

### Memory Model

Firmware code favors fixed-size containers and predictable memory behavior. Prefer ETL types such as `etl::unordered_map`, `etl::string`, and `etl::vector` with explicit compile-time limits.

## Testing Notes

- Desktop tests use Catch2 with mocks in `src/lib/mocks/` for FreeRTOS, pico-sdk, and hardware APIs.
- Top-level desktop test aggregation is defined in `src/CMakeLists.txt`.
- When changing a module that already has tests, update or extend those tests in the sibling `*_tests/` directory.

## Key File Locations

| File | Purpose |
|------|---------|
| `src/lib/core/ace/basemodule.hpp` | `BaseModule` and lifecycle interfaces |
| `src/lib/core/ace/messages.hpp` | Core message type definitions |
| `src/lib/gatas_module.cmake` | Template/pattern for new modules |
| `src/pico/main.cpp` | Firmware entry point and module instantiation |
| `src/pico/gatas_default_config.ini` | Runtime configuration |
| `src/lib/mocks/` | Desktop test mocks |
| `/src/SystemGUI` | Web Interface |
| `doc/message-bus.md` | Message bus flow documentation |

## Adding a New Module

1. Create `src/lib/<name>/` with `ace/<name>.hpp` and `ace/<name>.cpp`
2. Follow an existing module pattern such as `src/lib/ogn/`
3. Add a `CMakeLists.txt` using `src/lib/gatas_module.cmake` as the template
4. Add the new subdirectory to `src/lib/CMakeLists.txt`
5. Register the module in `src/pico/gatas_default_config.ini`
6. Add or extend desktop tests when the module has logic that can be validated off-target

## Message Bus Diagram Maintenance

`doc/message-bus.md` contains a PlantUML component diagram of message flows. When message producers or consumers change:

1. Find senders: `grep -r "getBus().receive(" src/lib --include="*.cpp" -n`
2. Find receivers: `grep -r "void on_receive(const GATAS::" src/lib --include="*.hpp"`
3. Update only the affected flow sections instead of collapsing the document into a single large diagram

## Vendor Dependencies

Vendored libraries live under `src/vendor/` and include ETL, ArduinoJson, FANET, FLARM, gdl90, libcrc, libmodes, and others. External runtime dependencies such as Pico SDK and FreeRTOS are supplied via environment variables for firmware builds.
