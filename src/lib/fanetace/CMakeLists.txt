cmake_minimum_required(VERSION 3.10.0)

project(fanetace VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/fanetace.cpp
)

set(MODULE_TARGET_LINK
    utils
    fanet
)

set(PICO_EXTRA_LIBS
    fanet::fanet
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
