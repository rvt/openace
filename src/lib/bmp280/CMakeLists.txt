cmake_minimum_required(VERSION 3.10.0)

project(bmp280 VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/src/bmp280.cpp
)

set(MODULE_TARGET_LINK
    acespi
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
