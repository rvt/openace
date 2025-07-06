# FLARM Library

Both examples codes come from the GA/TAS project so essentially this library only contains `Flarm2024Packet packet`
But I think the example speak for themselse fairly well.

> The code and decryption routines were originally sourced from Pastebin and have been used under the assumption of fair use. I firmly believe that anti-collision protocols—especially in aviation, where human lives are at stake and diverse systems must coexist—should be open and freely accessible. In safety-critical domains like this, transparency and interoperability are not optional; they are essential. Ensuring that such protocols are available to both hobbyists and professionals alike contributes directly to a safer airspace for everyone.

### How to create a FLARM packet

```c++

    // Example building a packet...
    Flarm2024Packet packet;
    auto epochSeconds = CoreUtils::secondsSinceEpoch();
    auto ownship = ownshipPosition.load(etl::memory_order_acquire);

    packet.aircraftId(gaTasConfiguration.address);
    packet.messageType(0x02);
    packet.addressType(static_cast<uint8_t>(gaTasConfiguration.addressType));
    packet.stealth(gaTasConfiguration.stealth);
    packet.noTrack(gaTasConfiguration.noTrack);
    packet.epochSeconds(epochSeconds);
    packet.aircraftType( static_cast<uint8_t>(gaTasConfiguration.category));
    packet.altitude(ownship.altitudeHAE);
    packet.setPosition(ownship.lat, ownship.lon);
    packet.turnRate(ownship.hTurnRate);
    packet.groundSpeed(ownship.groundSpeed);
    packet.verticalSpeed(ownship.verticalSpeed);
    packet.groundTrack(ownship.course);
    packet.movementStatus(ownship.groundSpeed > GATAS::GROUNDSPEED_CONSIDERING_AIRBORN ? 2 : 1);

    // 
    uint32_t buffer[7];
    packet.writeToBuffer(epochSeconds, buffer);

    // Buffer now holds the data that can be send by your tranceiver
```


### How to decode a FLARM packet:

msg.frame is an `uint32_t frame[7]`, this usually comes from your tranceiver.

```c++
  auto epochSeconds = CoreUtils::secondsSinceEpoch();

    Flarm2024Packet packet;
    auto ownship = ownshipPosition.load(etl::memory_order_acquire);
    auto result = packet.loadFromBuffer(epochSeconds, {frame, Flarm2024Packet::TOTAL_LENGTH_WORDS});
    if (result == -1) {
        statistics.crcErr++;
        return;
    } else if (result != 0) {
        // LSB seconds error not matched, or any other
        return;
    }

    if ( packet.messageType() != 0x02) {
        statistics.messageTypeNot0x02++;
        return;
    }

    // packet old's all information of the other aircraft
```