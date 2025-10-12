



## How to setup the development environment

First setup your development environment:

* [Pico Getting Started](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf)
* [Pico SDK](https://www.raspberrypi.com/documentation/pico-sdk/misc.html#boot_uf2)
* [Pico on a MAC](https://blog.smittytone.net/2021/02/02/program-raspberry-pi-pico-c-mac/)
* [Pico Probe scroll to page 60](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf)
* [FreeRTOS on RP2040](https://github.com/FreeRTOS/FreeRTOS-Community-Supported-Demos/tree/1ba24b127f11544263209a65f49cffdd7e42f04a/CORTEX_M0%2B_RP2040)
* [uBlox UBX](https://content.u-blox.com/sites/default/files/documents/u-blox-F9-HPG-1.32_InterfaceDescription_UBX-22008968.pdf)
* [On the Security of the FLARM Collision Warning System](https://www.researchgate.net/profile/Martin-Strohmeier-3/publication/360948435_On_the_Security_of_the_FLARM_Collision_Warning_System/links/6299473a55273755ebcd3db6/On-the-Security-of-the-FLARM-Collision-Warning-System.pdf)
* [On the Security of the FLARM Collision Warning System](https://wangboya.org/assets/pdf/Flarm_ASIACCS_camera_ready.pdf)
* [Modelling and Simulation of Collaborative Surveillance for Unmanned Traffic Management](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC8877271/)
* [OGN Tracking Protocol](http://wiki.glidernet.org/ogn-tracking-protocol#:~:text=spot%20a%20mistake.-,Modulation,bit%20rate%20is%2050%20kbps.)
* [OGN ARS Protocol](https://github.com/glidernet/ogn-aprs-protocol)
* [Geoid Height Calculator](https://www.researchgate.net/publication/358683481_Modelling_and_Simulation_of_Collaborative_Surveillance_for_Unmanned_Traffic_Management)
* [retrieve-uart-characters-in-a-freertos](https://community.element14.com/technologies/test-and-measurement/b/blog/posts/mini-project-pico-pi-programmable-lab-switch---1b-retrieve-uart-characters-in-a-freertos-task-with-interrupt-support)
* [Responsive WebAPP](https://github.com/sysprogs/PicoHTTPServer/)
* Instead of ArduinoJson?? [zcbor](https://github.com/NordicSemiconductor/zcbor)
* [FAT] https://github.com/carlk3/FreeRTOS-FAT-CLI-for-RPi-Pico/tree/master
https://www.everythingrf.com/community/gps-frequency-bands
https://forums.radioreference.com/threads/online-mode-s-ads-b-packet-decoder.402808/
* [btStack](https://bluekitchen-gmbh.com/btstack/#)


https://nl.aliexpress.com/item/1005007000015587.html?spm=a2g0o.productlist.main.1.35f877afniMYsr&algo_pvid=5250507e-5107-431d-a471-62c1119a5fce&algo_exp_id=5250507e-5107-431d-a471-62c1119a5fce-2&pdp_npi=4%40dis%21EUR%2164.55%2121.79%21%21%21488.28%21164.82%21%4021038dfc17284063069876204e5a02%2112000039006282929%21sea%21NL%211992146471%21X&curPageLogUid=nhaaADtDEbh7&utparam-url=scene%3Asearch%7Cquery_from%3A

Antenna:
https://www.kolins.cz/dipole-antenna-with-balun/
https://blog.minicircuits.com/demystifying-transformers-baluns-and-ununs/

### Installing the Tools

First, though this is how you set up your Pico toolchain on a Mac. The Foundation documents this, but different parts of the setup are covered in different places, so here is my combined, tested sequence. As an example, I’ll use the directory ~/git as the base installation location and the project name PicoTest, but you can use whatever paths and names you prefer, of course.

* Install the SDK:
  * In Terminal, go to your projects directory, eg. /opt/pico
  * Run git clone -b master --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
  * Edit your .bash_profile or .zshrc and add: export PICO_SDK_PATH="/opt/pico/pico-sdk"
* Install Free-RTOS
  * cd /opt/pico
  * git clone https://github.com/FreeRTOS/FreeRTOS-Kernel
  * Edit your .bash_profile or .zshrc and add: export FREERSTOS_KERNEL_PATH="/opt/pico/FreeRTOS-Kernel"

* Install the toolchain, don't use brew it seems to be broken:
  * Download toolchain from [Arm GNU Toolchain](https://developer.arm.com/downloads/-/gnu-rm)
  * Add export PATH=/Applications/ARM/bin:$PATH to you .zshrc .bash_profile 
  * Add export PICO_TOOLCHAIN_PATH="/Applications/ARM"
* Configure the IDE:
  * Run — or install and then run — Microsoft Visual Studio Code.
  * Click on the Extensions icon.
  * Enter CMake Tools in the search field.
  * Locate CMake Tools by Microsoft and click Install.
* From within Visual Studio Code, open the folder src.
* When CMakeTools asks you if to configure project, say yes.
* When asked to select a kit, select GCC for arm-none-eabi x.y.z.

Install OpenOCD (Strongly recommended if you want to develop on this board) from https://github.com/xpack-dev-tools/openocd-xpack/releases the one from brew did not work
and set "cortex-debug.openocdPath": `<your install path>/bin/openocd` in `settings.json`

install `Cortex-Debug` in VSC
Create a launch configuration:

```json
        { "name": "Pico Debug",
        "device": "RP2040",
        "gdbPath": "arm-none-eabi-gdb",
        "cwd": "${workspaceRoot}",
        "executable": "${command:cmake.launchTargetPath}",
        "request": "launch",
        "type": "cortex-debug",
        "servertype": "openocd",
        "configFiles": [
          "interface/cmsis-dap.cfg",
          "target/rp2040.cfg"
        ],
        "svdFile": "${env:PICO_SDK_PATH}/src/rp2040/hardware_regs/RP2040.svd",
        "runToEntryPoint": "",
        "showDevDebugOutput": "raw",
        "postRestartCommands": [
          "break main",
          "continue"
        ]
      },
```

If you get something like _adapter speed_ then add `adapter speed 12000` to `target/rp2040.cfg` (in openocd configuration directory) 

### Setup Visual Studio Code Multiroot

Multiroot is needed if you want to load specific test cases and run them from Visual C++

[Multiroot](https://code.visualstudio.com/docs/editor/multi-root-workspaces)

### Terminals I use

To see the printf statements during debugging

* [Chrome Terminal](https://webserial.io/) Seems to work very nice
* `minicom -D /dev/cu.usbmodem14102`, On  quickly press `esc q` to exit.
* AirConnect testing `nc 192.168.1.1 2000`    Exit ctrl->c 
# Dump1090 testing `nc 172.23.255.59 30002 | ts '%.s'`  

# wont work well : socat TCP-LISTEN:30002,fork TCP:172.23.255.59:30002


### Rest CSV Interface

http://192.168.178.143/api/RtcModule.json
http://192.168.178.143/api/Flarm.json
http://192.168.178.143/api/ADSL.json
http://192.168.178.143/api/GpsDecoder.json
http://192.168.178.143/api/Tuner.json
http://192.168.178.143/api/Radio_0.json
http://192.168.178.143/api/UbloxM8N.json
http://192.168.178.143/api/StatCollector/FreeRTOSStack.json
http://192.168.178.143/api/StatCollector.json
http://192.168.178.143/api/Gdl90Service.json
http://192.168.178.143/api/GDLoverUDP.json
http://192.168.178.143/api/ADSL.json



curl "http://192.168.178.143/api/_Configuration/GDLoverUDP/ips.json"
curl -X POST -d '{"ip":[192,168,178,192],"port":5002}' "http://192.168.178.143/api/_Configuration/GDLoverUDP/ips/0.json"
curl -X POST -d '{"ip":[192,168,178,192],"port":4002}' "http://192.168.178.143/api/_Configuration/GDLoverUDP/ips.json"
curl -X POST -H "X-METHOD: DELETE" "http://192.168.178.143/api/_Configuration/GDLoverUDP/ips/2.json" -d ''
curl -X POST -d '{"port": "port8", "txEnabled": 1, "offset": 3000}' "http://192.168.178.143/api/_Configuration/Sx1262_0.json"


curl -X POST -d '{"port": "porta","compensation": 0}' "http://192.168.178.143/api/_Configuration/Bmp280.json"

curl "http://192.168.178.143/api/_Configuration/aircraft.json"
curl -X POST -d '{"callSign":"XX-XXX","category":"ReciprocatingEngine","addressType":"ADSL","address":0,"noTrack":false,"privacy":false,"protocols":["FLARM","OGN","ADSL"]}' "http://192.168.178.143/api/_Configuration/aircraft/XX-XXX.json"

curl -X POST -d '{"callSign":"XX-XXX","category":"ReciprocatingEngine","addressType":"ADSL","address":0,"privacy":1,"protocols":["OGN","ADSL","FLARM"]}' "http://192.168.178.143/api/_Configuration/aircraft/XX-XXX.json"


curl "http://192.168.178.143/api/_Configuration/aircraft.json"

curl "http://192.168.178.143/api/_Configuration/WiFiClient/0.json"
curl -X POST -d '{"ssid":"net_1","password":"anode89-jeep-?!"}' "http://192.168.178.143/api/_Configuration/WiFiClient/0.json"










## Contributing

1. Use the GitHub pull request system.
2. Make sure to follow to existing style (naming, indentation, etc.)
3. Write unit tests for any new functionality you add.
4. Be aware you're submitting your work under the repository's license.


## Other links

[Mode S](https://mode-s.org/decode/misc/preface.html)


## Scratch pad
//        #if defined(PICO_RP2040)

Free RAM https://forums.raspberrypi.com/viewtopic.php?t=347638


FreeRTOS/Demo/ThirdParty/Community-Supported-Demos/CORTEX_M0+_RP2040 


https://forums.raspberrypi.com/viewtopic.php?t=329406&sid=6837a40d449c8d51755402a44ce0d96f&start=25
int heapLeft() {
  char *p = malloc(256);   // try to avoid undue fragmentation
  int left = &__StackLimit - p;
  free(p);
  return left;
}


https://www.gitbook.com/
https://svelte.dev
Atomic Queue: https://forum.pjrc.com/index.php?threads/raspberry-pi-pico.65899/page-9




WebSerial
https://serial.huhn.me/

Note: SoftRF puts out OGN1
file:///opt/source/SoftRF-Moshe/software/app/Settings/settings1.html
Latest : $PSRFC,1,0,7,1,8,1,0,0,2,1,1,1,1,4,0,0,0,0,0*40
OGN1   : $PSRFC,1,0,1,1,1,1,0,0,2,1,0,1,1,4,0,0,0,0,0*4E
PIW    : $PSRFC,1,0,2,1,8,1,0,0,2,1,1,1,1,4,0,0,0,0,0*45
FANET  : $PSRFC,1,0,5,1,8,1,0,0,2,1,1,1,1,4,0,0,0,0,0*42


Latest+BLE_+GDL90 over BLE: $PSRFC,1,0,7,1,1,1,0,0,2,1,0,1,1,5,5,0,0,0,0*4C

### Can not find free FPB Comparator!

see:  [](https://forums.raspberrypi.com/viewtopic.php?t=343683)

To many breakpoints or breakpoints that are old/outdated
