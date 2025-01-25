cmake_minimum_required(VERSION 3.10.0)

set(MSG_PREFIX "libCRC |")
project(libcrc VERSION 0.0.0 LANGUAGES CXX)

option(BUILD_TESTS "Build unit tests" OFF)
option(NO_STL "No STL" OFF)

add_library(${PROJECT_NAME} STATIC
    src/lib_crc.cpp
)

target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>

        )

message("Vendor Including ${CMAKE_CURRENT_LIST_DIR}")

target_link_libraries(${PROJECT_NAME} 
#    pico_platform
)
