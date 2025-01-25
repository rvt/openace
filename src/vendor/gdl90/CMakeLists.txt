cmake_minimum_required(VERSION 3.10.0)

set(MSG_PREFIX "GDL90 |")

project(GDL90 VERSION 0.0.0 LANGUAGES C)

option(BUILD_TESTS "Build unit tests" OFF)
option(NO_STL "No STL" OFF)

add_library(${PROJECT_NAME} STATIC
    GDL90.cpp
)

target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

message("Vendor Including ${CMAKE_CURRENT_LIST_DIR}")


target_link_libraries(${PROJECT_NAME}
   etl::etl
)
