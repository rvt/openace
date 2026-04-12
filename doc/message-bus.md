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
    [RadioTunerRx]
    [RadioTunerTx]
}

rectangle "ADSB Input" {
    [Dump1090Client]
}

rectangle "Protocols" {
    [Ogn1]
    [Flarm2024]
    [ADSLAce]
    [FanetAce]
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

rectangle "Infrastructure" {
    [Idle]
    [Configuration]
    [WifiService]
    [Bmp280]
    [PicoRTC]
}

[AbstractGnss] --> [GpsDecoder] : GPSSentenceMsg
[GpsDecoder] --> [Ogn1] : OwnshipPositionMsg
[GpsDecoder] --> [Flarm2024] : OwnshipPositionMsg
[GpsDecoder] --> [ADSLAce] : OwnshipPositionMsg
[GpsDecoder] --> [FanetAce] : OwnshipPositionMsg
[GpsDecoder] --> [AdsbDecoder] : OwnshipPositionMsg
[GpsDecoder] --> [RadioTunerRx] : OwnshipPositionMsg
[GpsDecoder] --> [RadioTunerTx] : OwnshipPositionMsg
[GpsDecoder] --> [PicoRTC] : UtcTimeMsg
[Sx1262] --> [Ogn1] : RadioRxManchesterMsg
[Sx1262] --> [Flarm2024] : RadioRxManchesterMsg
[Sx1262] --> [ADSLAce] : RadioRxManchesterMsg\nRadioRxMsg
[Sx1262] --> [FanetAce] : RadioRxMsg
[Dump1090Client] ..> [AdsbDecoder] : ADSBMessageBinMsg\n(via BinaryReceiver)
[Ogn1] --> [AircraftTracker] : IngressAircraftPositionMsg
[Flarm2024] --> [AircraftTracker] : IngressAircraftPositionMsg
[ADSLAce] --> [AircraftTracker] : IngressAircraftPositionMsg\nIngressAircraftPositionsMsg
[FanetAce] --> [AircraftTracker] : IngressAircraftPositionMsg
[AdsbDecoder] --> [AircraftTracker] : IngressAircraftPositionMsg
[AircraftTracker] --> [GDL90Service] : EgressAircraftPositionMsg
[AircraftTracker] --> [DataPort] : EgressAircraftPositionMsg
[AircraftTracker] --> [ADSLAce] : EgressAircraftPositionsMsg
[AircraftTracker] --> [AdsbDecoder] : AdapativeRadiusMsg
[RadioTunerRx] --> [Sx1262] : RadioControlMsg
[RadioTunerRx] --> [RadioTunerTx] : RadioControlMsg
[RadioTunerTx] --> [Ogn1] : RadioTxPositionRequestMsg
[RadioTunerTx] --> [Flarm2024] : RadioTxPositionRequestMsg
[RadioTunerTx] --> [ADSLAce] : RadioTxPositionRequestMsg
[RadioTunerTx] --> [FanetAce] : RadioTxPositionRequestMsg
[RadioTunerTx] --> [AircraftTracker] : RadioTxPositionRequestMsg
[GDL90Service] --> [GdlOverUdp] : GdlMsg
[DataPort] --> [GatasConnectUdp] : DataPortMsg
[DataPort] --> [GatasConnectTcp] : DataPortMsg
[DataPort] --> [Bluetooth] : DataPortMsg
[DataPort] --> [AirConnect] : DataPortMsg
[Bmp280] --> [Ogn1] : BarometricPressureMsg

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
    [Sx1262]
    [Idle]
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
[GpsDecoder] --> [Sx1262] : GpsStatsMsg
[GpsDecoder] --> [Idle] : GpsStatsMsg

[GpsDecoder] --> [PicoRTC] : UtcTimeMsg

[AbstractGnss] --> [DataPort] : GPSSentenceMsg

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
    [RadioTunerRx]
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
    [RadioTunerRx] as RTRx2
}

package "Monitoring" {
    [GatasConnectUdp]
}

[RadioTunerRx] --> [Sx1262] : RadioControlMsg
[Sx1262] --> [Ogn1] : RadioRxManchesterMsg
[Sx1262] --> [Flarm2024] : RadioRxManchesterMsg
[Sx1262] --> [ADSLAce] : RadioRxManchesterMsg\nRadioRxMsg
[Sx1262] --> [FanetAce] : RadioRxMsg
[Dump1090Client] ..> [AdsbDecoder] : ADSBMessageBinMsg\n(via BinaryReceiver)

[Ogn1] --> [AircraftTracker] : IngressAircraftPositionMsg
[Flarm2024] --> [AircraftTracker] : IngressAircraftPositionMsg
[ADSLAce] --> [AircraftTracker] : IngressAircraftPositionMsg\nIngressAircraftPositionsMsg
[FanetAce] --> [AircraftTracker] : IngressAircraftPositionMsg
[AdsbDecoder] --> [AircraftTracker] : IngressAircraftPositionMsg

note bottom of RTRx2 : RadioTunerRx also receives\nIngressAircraftPositionMsg\nfor traffic-biased scheduling

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
    [RadioTunerRx]
}

package "Protocols" {
    [Ogn1]
    [Flarm2024]
    [ADSLAce]
    [FanetAce]
}

package "Tracking" {
    [AircraftTracker]
}

package "Radio Hardware" {
    [Sx1262]
}

[RadioTunerRx] --> [Sx1262] : RadioControlMsg
[RadioTunerRx] --> [RadioTunerTx] : RadioControlMsg
[RadioTunerTx] --> [Ogn1] : RadioTxPositionRequestMsg
[RadioTunerTx] --> [Flarm2024] : RadioTxPositionRequestMsg
[RadioTunerTx] --> [ADSLAce] : RadioTxPositionRequestMsg
[RadioTunerTx] --> [FanetAce] : RadioTxPositionRequestMsg
[RadioTunerTx] --> [AircraftTracker] : RadioTxPositionRequestMsg

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

```plantuml
' =====================================================================
' TIMER / CONFIG / WIFI
' =====================================================================
@startuml
title Infrastructure Messages

left to right direction

package "Timers" {
    [Idle]
}

package "Config" {
    [Configuration]
}

package "Network" {
    [WifiService]
}

package "Timer Consumers" {
    [AdsbDecoder]
    [WifiService] as WS2
    [AirConnect]
    [AircraftTracker]
    [Bmp280]
    [DataPort]
}

package "Config Consumers" {
    [Sx1262]
    [GdlOverUdp]
    [Ogn1]
    [ADSLAce]
    [FanetAce]
    [AdsbDecoder] as AD2
    [GpsDecoder]
    [GatasConnectUdp]
    [GatasConnectTcp]
    [RadioTunerRx]
    [RadioTunerTx]
    [AircraftTracker] as AT2
    [GDL90Service]
    [Bmp280] as BMP2
}

package "Wifi Consumers" {
    [GatasConnectUdp] as GU2
    [GatasConnectTcp] as GT2
    [GdlOverUdp] as GO2
    [DataPort] as DP2
    [Dump1090Client]
    [AirConnect] as AC2
    [Idle] as I2
}

[Idle] --> [AdsbDecoder] : Every5SecMsg
[Idle] --> [WS2] : Every5SecMsg
[Idle] --> [AirConnect] : Every5SecMsg
[Idle] --> [AircraftTracker] : Every5SecMsg
[Idle] --> [Bmp280] : Every30SecMsg
[Idle] --> [DataPort] : Every30SecMsg

[Configuration] --> [Sx1262] : ConfigUpdatedMsg
[Configuration] --> [GdlOverUdp] : ConfigUpdatedMsg
[Configuration] --> [Ogn1] : ConfigUpdatedMsg
[Configuration] --> [ADSLAce] : ConfigUpdatedMsg
[Configuration] --> [FanetAce] : ConfigUpdatedMsg
[Configuration] --> [AD2] : ConfigUpdatedMsg
[Configuration] --> [GpsDecoder] : ConfigUpdatedMsg
[Configuration] --> [GatasConnectUdp] : ConfigUpdatedMsg
[Configuration] --> [GatasConnectTcp] : ConfigUpdatedMsg
[Configuration] --> [RadioTunerRx] : ConfigUpdatedMsg
[Configuration] --> [RadioTunerTx] : ConfigUpdatedMsg
[Configuration] --> [AT2] : ConfigUpdatedMsg
[Configuration] --> [GDL90Service] : ConfigUpdatedMsg
[Configuration] --> [BMP2] : ConfigUpdatedMsg

[WifiService] --> [GU2] : WifiConnectionStateMsg
[WifiService] --> [GT2] : WifiConnectionStateMsg
[WifiService] --> [GO2] : WifiConnectionStateMsg\nAccessPointClientsMsg
[WifiService] --> [DP2] : WifiConnectionStateMsg
[WifiService] --> [Dump1090Client] : WifiConnectionStateMsg
[WifiService] --> [AC2] : WifiConnectionStateMsg
[WifiService] --> [I2] : WifiConnectionStateMsg

@enduml
```
