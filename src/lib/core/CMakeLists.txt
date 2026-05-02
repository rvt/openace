cmake_minimum_required(VERSION 3.18)

project(core VERSION 0.0.0 LANGUAGES CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)

gatas_add_module(
    SOURCE_FILES
        ace/src/models.cpp
        ace/src/basemodule.cpp
        ace/src/coreutils.cpp
        ace/src/lib_crc.cpp
    TARGET_LINK_LIBS
        ArduinoJson
    PICO_EXTRA_LIBS
        pico_flash
        pico_cyw43_arch_lwip_sys_freertos
        hardware_flash
)
