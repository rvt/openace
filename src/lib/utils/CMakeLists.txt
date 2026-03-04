cmake_minimum_required(VERSION 3.10.0)

project(utils VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
        ace/src/picopio.cpp
        ace/src/ldpc.cpp
        ace/src/bitutils.cpp
        ace/src/encryption.cpp
        ace/src/EMA.cpp
        ace/src/ognconv.cpp
        ace/src/moreutils.cpp
        ace/src/manchester.cpp
        ace/src/cobs.cpp
)

set(PICO_EXTRA_LIBS
    hardware_pio
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
