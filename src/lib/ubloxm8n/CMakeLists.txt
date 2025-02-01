cmake_minimum_required(VERSION 3.10.0)

project(ubloxm8n VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/ubloxm8n.cpp
    ace/AbstractGnss.cpp
)

set(MODULE_TARGET_LINK
    pioserial
    utils
)

set(PICO_EXTRA_LIBS
    hardware_pio
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../openace_module.cmake)
