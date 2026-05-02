cmake_minimum_required(VERSION 3.18)

project(config VERSION 0.0.0 LANGUAGES CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)

gatas_add_module(
    SOURCE_FILES
        ace/src/config.cpp
        ace/src/flashstore.cpp
    TARGET_LINK_LIBS
        ArduinoJson
        utils
    PICO_EXTRA_LIBS
        pico_flash
        hardware_flash
        pico_rand
)
