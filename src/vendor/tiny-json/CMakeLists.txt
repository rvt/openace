cmake_minimum_required(VERSION 3.10.0)

set(MSG_PREFIX "TinyJson |")

project(TinyJson VERSION 0.0.0 LANGUAGES C)

option(BUILD_TESTS "Build unit tests" OFF)
option(NO_STL "No STL" OFF)

add_library(${PROJECT_NAME} STATIC
    tiny-json.c
)

target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

message("Vendor Including ${CMAKE_CURRENT_LIST_DIR}")
