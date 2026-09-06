cmake_minimum_required(VERSION 3.18)

project(gps VERSION 0.0.0 LANGUAGES CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)

gatas_add_module(
    SOURCE_FILES
        ace/src/ubloxm8n.cpp
        ace/src/L76B.cpp
        ace/src/AbstractGnss.cpp
        ace/src/NtpClient.cpp
        ace/src/StaticGPS.cpp
    TARGET_LINK_LIBS
        pioserial
        utils
    PICO_EXTRA_LIBS
        hardware_pio
        pico_cyw43_arch_lwip_sys_freertos
)
