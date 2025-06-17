cmake_minimum_required(VERSION 3.10.0)

project(gpsdecoder VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/gpsdecoder.cpp
)

set(MODULE_TARGET_LINK
    minmea
    utils
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
