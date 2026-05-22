# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Initial GATAS Companion support with a new `GatasConnect` core module and a separate `GatasConnectUDP` transport module.
- Bluetooth transport for GATAS Connect, including dedicated NMEA and binary GATT characteristics for companion-device integration.
- Optional GDL90-over-Bluetooth bridge using COBS-framed payloads for companion applications.
- Debug-only FreeRTOS queue registry entries for mutexes, to improve visibility in `GATAS_DEBUG` builds.
- Frontend unit-test support for `SystemGUI` via `npm test`.
- Option to show the datasource in the aicrafts callsign

### Changed

- Split GATAS Connect so COBS framing and request generation live in `GatasConnect`, while UDP transport is handled by `GatasConnectUDP`.
- Extended the web UI to configure GATAS Connect output, pin code, and GDL90 bridge settings, plus a separate UDP server configuration module.
- Improved module monitoring to render structured object and array values more clearly.
- Updated the bundled device database.
- Reworked `vDiagnosticsTask()` so task runtime statistics are reported from `uxTaskGetSystemState()` directly, with clearer boot-time and recent-window CPU metrics for SMP builds.
- Refactored `CoreUtils` from a static utility class into a namespace, while keeping mutable internal state private to the implementation file.
- Bluetooth advertising now splits the local name between the primary advertisement and scan response payloads, improving visibility of longer device names while keeping the custom service UUID advertised.
- Refined `GatasConnect` web configuration so the GDL90 bridge option is only shown for Bluetooth output modes, and documented frontend test usage.
- Optimised SX1262 protocol reconfiguration by tracking the currently programmed protocol and modulation, avoiding unnecessary full radio reconfiguration while keeping explicit standby mode selection.

### Deprecated

-

### Removed

-

### Fixed

- Fixed misleading task runtime reporting in diagnostics, where formatted runtime text could be mismatched against sorted task names.
- Fixed diagnostics labels and notes so CPU usage on the dual-core RP2040 SMP build is presented more honestly.
- Fixed aircraft tracker antenna polar output so it only reports radio-backed data sources, avoiding invalid transport-category entries in the UI.
- Fixed SX1262 LoRa RX bandwidth mapping for 500 kHz channels and capped TX power correctly at the radio maximum.
- Fixed radio receive statistics for FLARM, OGN so polar/range tracking only counts valid in-range packets.

### Security

-

## [v2.1.1] - 2026-04-30

### Added

-

### Changed

-

### Deprecated

-

### Removed

-

### Fixed

- Better rendering of RadioTUnerTX and Protocol Timing in the UI
- Moved the 'action' buttons to a seperate page in the UI

### Security

-

## [v2.1.0] - 2026-04-28

### Added

-

### Changed

- FLARM packets will now be corrected for bit flips when possible to improve reception on larger distances

### Deprecated

-

### Removed

-

### Fixed

- AntennaRadionPattern was alligned to north instead of track of aircraft. Only issue in the web interface

### Security

-

## [v2.0.1] - 2026-04-23

### Added

- Prefer radio-derived positions over ADS-B when an aircraft is received from multiple data sources. Only switch to ADS-B if the radio position is older than
  4,000,000 µs (4 seconds). This avoids sudden position jumps, as ADS-B data is typically delayed. And we trust radio positions more.

### Changed

-

### Deprecated

-

### Removed

-

### Fixed

-

### Security

-

## [v2.0.0] - 2026-04-13

### Added

- ADSL Traffic Uplink on O-Band for both reception and transmission (in ground station mode)
- Groundstation mode that can show a static object at any height.
- Send Traffic over ADSL using uplinkTraffic message
- Low power mode during development
- Allow for per protocol RX or TX, or RX and TX selection
- Show in aircraft tracker the aircraft that it is tracking
- Adaptive protocol prioritisation. This will allocate more listening slots for protocols that is actually received to increase pings per aircraft

### Changed

- When GATAS is Stationary, reduce TX times for positional data to average of 5..6 seconds
- Use etl::delegate instead of etl::function
- Stopped storing a 'GATAS::*Msg' in local objects, always copy the contents as a rule
- Update list of possible Aircraft types in UI
- Update DDB
- Better represent RX Schedule with multiple protocols
- Use xTaskNotifyWait instead of ulTaskNotifyTake
- Removed the mutexes from the MessageRouter

### Deprecated

-

### Removed

-

### Fixed

- Regression on FANET TX
- Possible zero or negative values in FreeRTOS delays
- Fixed Incorrectly handle of device error on transceiver

### Security

-

## [v1.2.0] - 2026-03-12

### Added

- Show polar diagrams one ranges per each protcol on AircraftTracker

### Changed

- Update on the web interface where dimensions are shown instead of raw numbers
- Update on the aircraft database

### Fixed

- GDL90 Bug where negative lat/log where not resolved correctly on output

## [v1.1.0] - 2026-03-02

- Initial release of GATAS
