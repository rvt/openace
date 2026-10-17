# Local BLE device actions

This extension adds Save configuration, watchdog Restart and USB bootloader entry
for Companion clients. The paired Watch may request Save and Restart through the
iPhone; USB boot remains phone/Android only. There is no UF2 upload or OTA update.

## Wire contract

Existing COBS framing and its terminating zero are unchanged. Integers are big
endian. Type 12 is a 16-byte decoded request:

| Offset | Size | Value |
| --- | --- | --- |
| 0 | 1 | DEVICE_ACTION_REQUEST_V1 = 12 |
| 1 | 1 | Status=0, Save=1, Restart=2, UsbBoot=3 |
| 2 | 1 | Prepare=0, Execute=1 |
| 3 | 4 | Nonzero request ID |
| 7 | 8 | Expected boot ID (zero for Status) |
| 15 | 1 | RequireSaved=0, SaveFirst=1, AllowUnsaved=2 |

Non-USB policy must be zero. Status only accepts Prepare and has no side effects.
Type 13 is a 17-byte decoded response:

| Offset | Size | Value |
| --- | --- | --- |
| 0 | 1 | DEVICE_ACTION_RESPONSE_V1 = 13 |
| 1 | 1 | Action |
| 2 | 4 | Echoed request ID |
| 6 | 1 | Result |
| 7 | 1 | Capability bits: Save=1, Restart=2, USB boot=4 |
| 8 | 8 | Current nonzero boot ID |
| 16 | 1 | Unsaved configuration, 0/1 |

Result values: Ok=0, Accepted=1, Unsupported=2, Invalid=3, Busy=4, SaveFailed=5,
StaleBoot=6, UnsavedChanges=7, Expired=8. New clients probe Status on each BLE
connection; no response from older firmware disables the new controls only.

Shared raw fixtures (also asserted by Kotlin tests):

```text
Save Prepare: 0c010012345678010203040506070800
Save result:  0d01123456780007010203040506070801
```

## Execution and routing

Prepare reserves one action for ten seconds. It does not save or reset. A client
must receive Accepted before sending Execute with the identical operation, ID,
boot ID and policy. Reset may then occur immediately: unlike an ACK in a BLE
send buffer, the Execute establishes that the app received acceptance.

The coordinator keeps 16 results. Duplicates return the cached result without
executing again; conflicting ID reuse, wrong boot IDs, phases or policies fail.
Boot IDs change on each start. Disconnect invalidates prepared actions. An ID
outside the bounded cache must not be reused by clients within the same session.

Bluetooth assigns a generation to each connection. Internal Rx, request,
response and Tx carry that generation, and Tx selects only its originating BLE
connection. Management is handled before legacy holdback, never on UDP ingress,
and never routed to the Internet. Fragments cannot span source/session changes.
See [message-bus.md](message-bus.md) for typed routes. Config owns state; new
modules retain no Configuration reference.

## Configuration persistence

One recursive RAII mutex serializes web/BLE mutation, status and SaveBr/Save.
Typed BLE aircraft selection updates dirty state and retained RAM. The checked
save verifies capacity, complete write count and flash readback before declaring
success or changing save statistics. Identical saved data avoids another flash
write. Failures preserve the current RAM configuration and report SaveFailed.
Flash safe-execute waits at most five seconds instead of forever.

Normal Restart uses the existing watchdog and the reboot-retained RAM store.
It does not automatically save flash. USB SaveFirst requires successful verified
saving; RequireSaved checks again at execution and rejects intervening web edits;
AllowUnsaved proceeds without saving. Temporary AP/CLIENT mode does not become a
saved startup mode. Platform reset calls are isolated for desktop tests.

The existing single flash bank is not power-loss-atomic. This extension does not
change its storage layout. A failed write is reported; surviving arbitrary power
loss during erase/program would require a separate transactional store design.

## Validation

Desktop tests cover protocol fixtures, malformed input, duplicate/conflicting
IDs, expiration, boot/session mismatch, disconnect, concurrent preparation,
bounded cache, holdback/UDP isolation and cross-session frame fragments. Config
tests inject zero/partial/corrupt writes, verify capacity and no-op saves, race
web changes against BLE saves/status, and simulate warm RAM and cold flash loads.
These mocks do not validate real flash timing, RAM retention or ROM USB entry.

On 2026-10-17, all 26 desktop test executables passed (226 test cases, 13,313
assertions). Release ELF/UF2 builds succeeded for RP2040/pico_w and
RP2350/pico2_w using the repository Docker toolchain, Pico SDK 2.1.1 and FreeRTOS
11.2. `rtk` 1.0.1 was provisioned as a temporary local binary without installing
global shell hooks. Firmware and Companion changes remain on their own
`codex/device-actions` branches, based on f2bc3390 and 38272c7 respectively.

Physical acceptance still requires RP2040/RP2350 hardware: change an unsaved test
value and aircraft, watchdog restart, verify retention; save and power-cycle,
verify persistence; reconnect through the app and check a new boot ID; test both
USB save decisions and failed-save prevention. Repeat Watch Save/Restart with
locked iPhone, no Internet, BLE interruptions and simultaneous phone use.
The app must report USB mode as requested, never infer bootloader success solely
from a disconnect. No control command may replay after reconnection.
