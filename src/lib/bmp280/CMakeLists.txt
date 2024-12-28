cmake_minimum_required(VERSION 3.5.0)

project(bmp280 VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/bmp280.cpp
)

set(MODULE_TARGET_LINK
    acespi
)

set(PICO_EXTRA_LIBS
    hardware_spi
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../openace_module.cmake)
