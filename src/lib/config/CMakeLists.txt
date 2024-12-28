cmake_minimum_required(VERSION 3.5.0)

project(config VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/config.cpp
    ace/inmemorystore.cpp
    ace/flashstore.cpp
)

set(MODULE_TARGET_LINK
    ArduinoJson    
)

set(PICO_EXTRA_LIBS
    pico_cyw43_arch_lwip_sys_freertos
    pico_flash
    hardware_flash
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../openace_module.cmake)
