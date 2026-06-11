# WifiService State Machine

This document describes the current `WifiService::wifiTask()` state machine in
`src/lib/wifiservice/ace/src/wifiservice.cpp`.

## Notes

- `requestedWifiMode` is updated externally through `WifiModeRequestMsg` or
  `setData(..., "startAp")`.
- The task wakes roughly once per second and advances the state machine.
- The diagram below reflects the current implementation, including the current
  transition targets in code.

## PlantUML

```plantuml
@startuml
title WifiService State Machine

hide empty description
top to bottom direction

[*] --> START

state START
state ENABLECLIENT
state WIFISCAN
state WIFISCANNING
state TRYCLIENTCONNECT
state CLIENTMODESTARTED
state APMODESTART
state APSTARTED
state AP_TO_CLIENT
state CLIENT_TO_AP

START : wifiMode = NC
START --> APMODESTART : no configured client networks
START --> ENABLECLIENT : client networks configured\nreset totalScanAttempt

ENABLECLIENT : if no clients configured:\nrequestedWifiMode = AP
ENABLECLIENT : enable client mode
ENABLECLIENT --> WIFISCAN

WIFISCAN : publish WifiConnectionStateMsg(NC)
WIFISCAN : start WiFi scan\nset scan timeout
WIFISCAN : if dontScanJustConnectToClient:\nfill scanResult from config\nset TRYCLIENTCONNECT first
WIFISCAN --> WIFISCANNING

WIFISCANNING --> TRYCLIENTCONNECT : scan completed
WIFISCANNING --> APMODESTART : scan timeout\nleave WiFi + disable client mode

TRYCLIENTCONNECT : leave current WiFi association before connect attempt
TRYCLIENTCONNECT --> CLIENT_TO_AP : requestedWifiMode == AP
TRYCLIENTCONNECT --> CLIENTMODESTARTED : connectClient() == CONNECTED\nmDNS init on STA\nsuccessClientConnected = true
TRYCLIENTCONNECT --> TRYCLIENTCONNECT : connectClient() == MORE
TRYCLIENTCONNECT --> ENABLECLIENT : connectClient() == EXHAUSTED\nand totalScanAttempt reaches 0
TRYCLIENTCONNECT --> START : connectClient() == EXHAUSTED\nand (apDisabled or successClientConnected)
TRYCLIENTCONNECT --> CLIENT_TO_AP : connectClient() == EXHAUSTED\nand AP fallback allowed

CLIENTMODESTARTED --> WIFISCAN : client link lost\nrelease/restart DHCP\nmDNS deinit on STA

APMODESTART : disable client mode\nleave WiFi\nstart AP\nmDNS init on AP
APMODESTART --> APSTARTED

APSTARTED --> AP_TO_CLIENT : requestedWifiMode == CLIENT
APSTARTED : every ~5 ticks\nhandleAccesspointClients()

AP_TO_CLIENT : mDNS deinit on AP\nstop AP\nwifiMode = NC
AP_TO_CLIENT --> START

CLIENT_TO_AP : stopClient()
CLIENT_TO_AP --> APMODESTART

START -[hidden]down-> ENABLECLIENT
ENABLECLIENT -[hidden]down-> WIFISCAN
WIFISCAN -[hidden]down-> WIFISCANNING
WIFISCANNING -[hidden]down-> TRYCLIENTCONNECT
TRYCLIENTCONNECT -[hidden]down-> CLIENTMODESTARTED
CLIENTMODESTARTED -[hidden]down-> APMODESTART
APMODESTART -[hidden]down-> APSTARTED
APSTARTED -[hidden]down-> AP_TO_CLIENT
AP_TO_CLIENT -[hidden]down-> CLIENT_TO_AP

note right of TRYCLIENTCONNECT
  connectClient() returns:
  - CONNECTED
  - MORE
  - EXHAUSTED
end note

note right of WIFISCAN
  Current code always ends this state
  in WIFISCANNING, even when
  dontScanJustConnectToClient is true.
end note

note right of APSTARTED
  CLIENT requests are only acted on
  from APSTARTED.
end note

note right of TRYCLIENTCONNECT
  AP requests are only acted on
  from TRYCLIENTCONNECT.
end note

@enduml
```

## Decision Summary

- If no client networks are configured, startup goes directly to AP mode.
- If client networks are configured, startup first tries STA/client mode.
- `ENABLECLIENT` also collapses `requestedWifiMode` back to `AP` when the client
  list is empty.
- `ENABLECLIENT` currently always transitions into `WIFISCAN`.
- `WIFISCAN` currently always ends in `WIFISCANNING`; when
  `dontScanJustConnectToClient` is true it also pre-fills `scanResult` first.
- A request for AP mode is currently handled in `TRYCLIENTCONNECT`.
- A request for client mode is currently handled in `APSTARTED`.
- Loss of an active client connection currently goes directly back into
  `WIFISCAN`.
