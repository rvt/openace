cmake_minimum_required(VERSION 3.10.0)

project(utils VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/utils.cpp
    ace/ldpc.cpp
    ace/bitcount.cpp
    ace/encryption.cpp
    ace/EMA.cpp
    ace/ognconv.cpp
    ace/moreutils.cpp
    ace/manchester.cpp
)

set(PICO_EXTRA_LIBS
    hardware_pio
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../openace_module.cmake)
