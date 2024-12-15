cmake_minimum_required(VERSION 3.5.0)

project(dataport VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/dataport.cpp
)

set(MODULE_TARGET_LINK
    utils
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../openace_module.cmake)
