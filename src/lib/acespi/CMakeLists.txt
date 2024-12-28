cmake_minimum_required(VERSION 3.5.0)

project(acespi VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/acespi.cpp
)

set(PICO_EXTRA_LIBS
    pico_cyw43_arch_lwip_sys_freertos
    hardware_spi
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../openace_module.cmake)
