# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

## 2.0.1 - 2026-04-32

### Added

- Prefer radio-derived positions over ADS-B when an aircraft is received from multiple data sources. Only switch to ADS-B if the radio position is older than 4,000,000 µs (4 seconds). This avoids sudden position jumps, as ADS-B data is typically delayed. And we trust radio positions more.

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

## 2.0.0 - 2026-04-13

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

## [1.2.0] - 2026-03-12

### Added

- Show polar diagrams one ranges per each protcol on AircraftTracker

### Changed

- Update on the web interface where dimensions are shown instead of raw numbers
- Update on the aircraft database

### Fixed

- GDL90 Bug where negative lat/log where not resolved correctly on output

## [1.1.0] - 2026-03-02

- Initial release of GATAS
