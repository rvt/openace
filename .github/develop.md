# Development version

This is the **DEVELOPMENT** version of GA/TAS, use at your own risk

> [!TIP]
> **GA/TAS is an ongoing project** that is continuously being developed and improved.
> As a result, features, functionality, and performance may be updated or modified at any time without prior notice.
> We value feedback and collaboration! 
> If you have any questions, suggestions, or concerns, feel free to reach out. 
> Your input helps us improve the project and ensure it meets the needs of the community.

> [!IMPORTANT]
> **Disclaimer: Use at Your Own Risk**
> 
> GA/TAS is provided "as is," without any warranties.
> By downloading or using this device, you acknowledge that you do so at your own risk. 
> The creators are not liable for any damages or issues resulting from its use. 
> GA/TAS is intended for General Aviation only and should not be relied upon as the sole source of traffic or navigation information.
> Users are responsible for ensuring compliance with local aviation regulations.

## How to Copy the UF2 File to the Raspberry Pi Pico

### 1. Download the `GATAS_rp2040.uf2` or `GATAS_rp2350-arm-s.uf2` File
- Obtain the correct firmware suitable for your PICO from [https://github.com/rvt/OpenAce/release](https://github.com/rvt/OpenAce/release)
- Make sure you know where the file is saved on your computer.

### 2. Connect Raspberry Pi Pico to Your Computer
- **Hold down the BOOTSEL button** on your Raspberry Pi Pico.
- While holding the BOOTSEL button, **connect the Pico to your computer** using a USB cable.
- Once connected, release the BOOTSEL button.

### 3. Raspberry Pi Pico in USB Mass Storage Mode
- Your Pico will appear as a **USB mass storage device** on your computer (e.g., as `RPI-RP2`).
    - On **Windows**, it will appear as a removable drive.
    - On **macOS** and **Linux**, it will appear as an external drive.

### 4. Copy `GATAS_*.uf2` File to the Pico
- Open the `RPI-RP2` drive.
- **Drag and drop** or **copy and paste** the `GATAS_*.uf2` file into the Pico's drive.
    - Do **not** try to open or modify the `GATAS_*.uf2` file; simply copy it.

### 5. Automatic Reboot
- After the file is copied, the Pico will **automatically reboot** and run the new firmware.
- The Pico will disappear from your list of drives, indicating that the flashing process is complete.

### 6. Verify the Installation
- Once rebooted, the Pico will start running the newly flashed firmware, it should show a green flasing led

## Troubleshooting
- **If the Pico does not show up as a USB drive**:
    - Ensure you are holding the BOOTSEL button while connecting it to the computer.
    - Try using a different USB cable (some cables only provide power and don't support data transfer).

## Summary
1. Download the `GATAS_*.uf2` file.
2. Hold down the BOOTSEL button and connect the Pico to your computer.
3. Release the BOOTSEL button once connected.
4. Copy the `GATAS_*.uf2` file to the Pico's mass storage drive.
5. The Pico will reboot automatically, running the new firmware.
6. The green LED on the PICO will start flashing once a second