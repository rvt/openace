# GA/TAS Conspicuity Device PCB

> **_NOTE:_**:
> This PCB design contains few, but not all changes from the V0.0.1 version (not published) and is still work in progress.

> **_NOTE:_**:
> Help wanted. If you have some PCB experence and also into flying, I would love to have some help with this!

Except for the memory card and the items fixed below, all items on this PCB has been verified to be working on the prototype board V0.0.1


![KiCAD 3D Rendering](../doc/img/kicadpcb.jpg)

To open and work on the PCB you need to install [KiCAD](https://www.kicad.org) KiCAD is an openSource schema and PCB design program which was used to design the PCB and to manufacture the PCB.

## Todo for V0.0.2

| Item                                                                                                       | Comment                                                                                                        | Status |
|------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------|--------|
| S8050 has pin 1 and 2 rotated                                                                              |                                                                                                                | fixed  |
| LM3671 incorrectly has PIN 4 to GND                                                                        |                                                                                                                | fixed  |
| Flash Card is rotated                                                                                      | no comment...                                                                                                  | fixed  |
| Connection terminals too close                                                                             |                                                                                                                | fixed  |
| Remove breakout header                                                                                     | Safe space, not needed anyways...                                                                              |        |
| Use PICO 2350 (need to wait for WIFI version)                                                              | Faster CPU, floating point.                                                                                    |        |
| Add “Port” number on the PCB                                                                               | Might make people understand the board better                                                                  |        |
| Use MIC5219 instead of LM3671                                                                              | Price point, but need to validate                                                                              |        |
| Add silkscreen markers so pins are more clear                                                              |                                                                                                                |        |
| Add solder jumpers show what is the 5V side and 3.3V side                                                  |                                                                                                                |        |
| Put Positive and negative Battery connection to opposite end of the board                                  | to avoid longer connecte wires on the battery which can lead to shortcuts when assembling/working on the board |        |
| Battery charger is slightly too wide in length TP4056                                                      |                                                                                                                |        |
| Re-organize board: Pico in center facing ‘up’ with 868Mhz on left and right. GPS and ADSB under the 868Mhz | This will make the antenna more separate and make the board not so wide. Any other ideas?                      |        |
| Double switch as a PCB board item to have a battery charge, off and battery charge-on position             | Conserve battery, makes it easer to solder the board eg less wires and less chance of mistakes                 |        |
| Add + and - of battery connection to opposite of PCB instead of next to eachother                          | To ensure people create short wires on the battery to prevent shortcuts during testing/developing              |        |
| GPS chip should be able to be both internal and external                                                   |                                                                                                                |        |
| AHRS on the board?                                                                                         | people seem to want that..                                                                                     |        |
