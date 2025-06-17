cmake_minimum_required(VERSION 3.10.0)

project(gps VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/ubloxm8n.cpp
    ace/L76B.cpp
    ace/AbstractGnss.cpp
)

set(MODULE_TARGET_LINK
    pioserial
    utils
)

set(PICO_EXTRA_LIBS
    hardware_pio
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
