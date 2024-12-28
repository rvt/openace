cmake_minimum_required(VERSION 3.5.0)

project(gdl90service VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/gdl90service.cpp
)

set(MODULE_TARGET_LINK
    GDL90
)

set(PICO_EXTRA_LIBS
    pico_cyw43_arch_lwip_sys_freertos
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../openace_module.cmake)
