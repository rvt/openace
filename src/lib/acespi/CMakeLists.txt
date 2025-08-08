cmake_minimum_required(VERSION 3.10.0)

project(acespi VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/src/acespi.cpp
)

set(PICO_EXTRA_LIBS
    hardware_spi
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
