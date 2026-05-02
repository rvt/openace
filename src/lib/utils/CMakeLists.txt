cmake_minimum_required(VERSION 3.18)

project(utils VERSION 0.0.0 LANGUAGES CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)

gatas_add_module(
    SOURCE_FILES
        ace/src/picopio.cpp
        ace/src/ldpc.cpp
        ace/src/bitutils.cpp
        ace/src/encryption.cpp
        ace/src/EMA.cpp
        ace/src/ognconv.cpp
        ace/src/moreutils.cpp
        ace/src/manchester.cpp
        ace/src/cobs.cpp
    PICO_EXTRA_LIBS
        hardware_pio
)
