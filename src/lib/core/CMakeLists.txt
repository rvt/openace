cmake_minimum_required(VERSION 3.10.0)

project(core VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/models.cpp
    ace/basemodule.cpp
    ace/coreutils.cpp
)

set(MODULE_TARGET_LINK
    ArduinoJson
)

set(PICO_EXTRA_LIBS
    pico_flash
    pico_cyw43_arch_lwip_sys_freertos
    hardware_flash
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
