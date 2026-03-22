# OpenAce Message Bus

This document maps every message type to its sender(s) and receiver(s).

> **How this was generated**: Senders were found by grepping `getBus().receive(` across all `.cpp` files. Receivers were found by grepping `void on_receive(const GATAS::` across all `.hpp` files. The class name was inferred from the enclosing file/class context.


```plantuml
' =====================================================================
' SYSTEM OVERVIEW
' =====================================================================
@startuml
title System Overview

left to right direction

rectangle "GPS Subsystem" {
    [AbstractGnss]
    [GpsDecoder]
}

rectangle "Radio Subsystem" {
    [Sx1262]
}

rectangle "ADSB Input" {
    [Dump1090Client]
}

rectangle "Protocols" {
    [Radio Protocols]
    [AdsbDecoder]
}

rectangle "Tracking" {
    [AircraftTracker]
}

rectangle "Outputs" {
    [GDL90Service]
    [GdlOverUdp]
    [DataPort]
    [GatasConnectUdp]
    [GatasConnectTcp]
    [Bluetooth]
    [AirConnect]
}

[AbstractGnss] --> [GpsDecoder] : GPSSentenceMsg
[GpsDecoder] --> [Radio Protocols] : OwnshipPositionMsg
[Sx1262] --> [Radio Protocols] : RadioRxManchesterMsg\nRadioRxMsg
[Dump1090Client] ..> [AdsbDecoder] : ADSBMessageBinMsg\n(via BinaryReceiver)
[Radio Protocols] --> [AircraftTracker] : IngressAircraftPositionMsg
[AdsbDecoder] --> [AircraftTracker] : IngressAircraftPositionMsg
[AircraftTracker] --> [GDL90Service] : EgressAircraftPositionMsg
[AircraftTracker] --> [DataPort] : EgressAircraftPositionMsg
[AircraftTracker] --> [Radio Protocols] : EgressAircraftPositionsMsg
[AircraftTracker] --> [AdsbDecoder] : AdapativeRadiusMsg
[GDL90Service] --> [GdlOverUdp] : GdlMsg
[DataPort] --> [GatasConnectUdp] : DataPortMsg
[DataPort] --> [GatasConnectTcp] : DataPortMsg
[DataPort] --> [Bluetooth] : DataPortMsg
[DataPort] --> [AirConnect] : DataPortMsg

@enduml
```

```plantuml
' =====================================================================
' GPS DATA FLOW
' =====================================================================
@startuml
title GPS Data Flow

left to right direction

package "GPS" {
    [AbstractGnss]
    [GpsDecoder]
    [PicoRTC]
}

package "Consumers" {
    package "Protocols" {
        [Ogn1]
        [Flarm2024]
        [ADSLAce]
        [FanetAce]
        [AdsbDecoder]
    }
    [RadioTunerRx]
    [RadioTunerTx]
    [GDL90Service]
    [DataPort]
    [GatasConnectUdp]
    [GatasConnectTcp]
    [Bluetooth]
}

[AbstractGnss] --> [GpsDecoder] : GPSSentenceMsg
[GpsDecoder] --> [Ogn1] : OwnshipPositionMsg\nGpsStatsMsg
[GpsDecoder] --> [Flarm2024] : OwnshipPositionMsg
[GpsDecoder] --> [ADSLAce] : OwnshipPositionMsg\nGpsStatsMsg
[GpsDecoder] --> [FanetAce] : OwnshipPositionMsg
[GpsDecoder] --> [AdsbDecoder] : OwnshipPositionMsg
[GpsDecoder] --> [RadioTunerRx] : OwnshipPositionMsg
[GpsDecoder] --> [RadioTunerTx] : OwnshipPositionMsg
[GpsDecoder] --> [GDL90Service] : OwnshipPositionMsg\nGpsStatsMsg
[GpsDecoder] --> [DataPort] : OwnshipPositionMsg
[GpsDecoder] --> [GatasConnectUdp] : OwnshipPositionMsg\nGpsStatsMsg
[GpsDecoder] --> [GatasConnectTcp] : OwnshipPositionMsg\nGpsStatsMsg
[GpsDecoder] --> [Bluetooth] : OwnshipPositionMsg

[GpsDecoder] --> [PicoRTC] : UtcTimeMsg

@enduml
```

```plantuml

' =====================================================================
' RADIO RX FLOW
' =====================================================================
@startuml
title Radio RX Flow

left to right direction

package "Radio" {
    [Sx1262]
}

package "ADSB Input" {
    [Dump1090Client]
}

package "Protocol Decoders" {
    [Ogn1]
    [Flarm2024]
    [ADSLAce]
    [FanetAce]
    [AdsbDecoder]
}

package "Tracking" {
    [AircraftTracker]
}

[Sx1262] --> [Ogn1] : RadioRxManchesterMsg
[Sx1262] --> [Flarm2024] : RadioRxManchesterMsg
[Sx1262] --> [ADSLAce] : RadioRxManchesterMsg\nRadioRxMsg
[Sx1262] --> [FanetAce] : RadioRxMsg
[Dump1090Client] ..> [AdsbDecoder] : ADSBMessageBinMsg\n(via BinaryReceiver)

[Ogn1] --> [AircraftTracker] : IngressAircraftPositionMsg
[Flarm2024] --> [AircraftTracker] : IngressAircraftPositionMsg
[ADSLAce] --> [AircraftTracker] : IngressAircraftPositionMsg
[FanetAce] --> [AircraftTracker] : IngressAircraftPositionMsg
[AdsbDecoder] --> [AircraftTracker] : IngressAircraftPositionMsg

@enduml
```


```plantuml
' =====================================================================
' TRACKING AND OUTPUT
' =====================================================================
@startuml
title Aircraft Tracking and Output

left to right direction

[AircraftTracker]

package "Protocol Feedback" {
    [ADSLAce]
    [AdsbDecoder]
}

package "Output Services" {
    [GDL90Service]
    [GdlOverUdp]
    [DataPort]
    [GatasConnectUdp]
    [GatasConnectTcp]
    [Bluetooth]
    [AirConnect]
}

[AircraftTracker] --> [GDL90Service] : EgressAircraftPositionMsg
[AircraftTracker] --> [DataPort] : EgressAircraftPositionMsg
[AircraftTracker] --> [ADSLAce] : EgressAircraftPositionsMsg
[AircraftTracker] --> [AdsbDecoder] : AdapativeRadiusMsg

[GDL90Service] --> [GdlOverUdp] : GdlMsg

[DataPort] --> [GatasConnectUdp] : DataPortMsg
[DataPort] --> [GatasConnectTcp] : DataPortMsg
[DataPort] --> [Bluetooth] : DataPortMsg
[DataPort] --> [AirConnect] : DataPortMsg

@enduml
```

```plantuml
' =====================================================================
' RADIO TX FLOW
' =====================================================================
@startuml
title Radio TX Flow

left to right direction

package "Radio Control" {
    [RadioTunerTx]
}

package "Protocols" {
    [Ogn1]
    [Flarm2024]
    [ADSLAce]
    [FanetAce]
}

[RadioTunerTx] --> [Ogn1] : RadioTxPositionRequestMsg
[RadioTunerTx] --> [Flarm2024] : RadioTxPositionRequestMsg
[RadioTunerTx] --> [ADSLAce] : RadioTxPositionRequestMsg
[RadioTunerTx] --> [FanetAce] : RadioTxPositionRequestMsg

[Ogn1] --> [Sx1262] : RadioTxFrameMsg
[Flarm2024] --> [Sx1262] : RadioTxFrameMsg
[ADSLAce] --> [Sx1262] : RadioTxFrameMsg
[FanetAce] --> [Sx1262] : RadioTxFrameMsg

@enduml
```

```plantuml
' =====================================================================
' SENSOR FLOW
' =====================================================================
@startuml
title Sensor Data Flow

left to right direction

[Bmp280] --> [Ogn1] : BarometricPressureMsg

@enduml
```
