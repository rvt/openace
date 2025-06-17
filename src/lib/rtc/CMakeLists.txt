cmake_minimum_required(VERSION 3.10.0)

project(rtc VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/picortc.cpp
)

set(MODULE_TARGET_LINK
    libcrc
    utils
)

set(PICO_EXTRA_LIBS
    pico_time
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
