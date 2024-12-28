cmake_minimum_required(VERSION 3.5.0)

project(serialadsb VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/serialadsb.cpp
)

set(MODULE_TARGET_LINK
    libcrc
    utils
    pioserial

)

set(PICO_EXTRA_LIBS
    hardware_pio
)


include(${CMAKE_CURRENT_SOURCE_DIR}/../openace_module.cmake)
