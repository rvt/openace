cmake_minimum_required(VERSION 3.18)

project(adsbdecoder VERSION 0.0.0 LANGUAGES CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)

gatas_add_module(
    SOURCE_FILES
        ace/src/adsbdecoder.cpp
        ace/src/cpr.cpp
    TARGET_LINK_LIBS
        utils
        libmodes
)
