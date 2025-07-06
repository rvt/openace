cmake_minimum_required(VERSION 3.10.0)

project(flarm VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/flarm2024.cpp
)

set(MODULE_TARGET_LINK
    utils
    flarm
)

set(PICO_EXTRA_LIBS
    flarm::flarm
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
