cmake_minimum_required(VERSION 3.10.0)

project(config VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
        ace/src/config.cpp
        ace/src/flashstore.cpp
)

set(MODULE_TARGET_LINK
    ArduinoJson
    utils
)

set(PICO_EXTRA_LIBS
    pico_flash
    hardware_flash
    pico_rand
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
