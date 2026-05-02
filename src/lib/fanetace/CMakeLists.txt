cmake_minimum_required(VERSION 3.18)

project(fanetace VERSION 0.0.0 LANGUAGES CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)

gatas_add_module(
    SOURCE_FILES
        ace/src/fanetace.cpp
    TARGET_LINK_LIBS
        utils
        fanet
    PICO_EXTRA_LIBS
        fanet::fanet
)
