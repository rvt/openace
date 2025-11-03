cmake_minimum_required(VERSION 3.10.0)

project(idle VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/src/idle.cpp
)

set(MODULE_TARGET_LINK
    acespi
)

set(PICO_EXTRA_LIBS
    pico_cyw43_arch_lwip_sys_freertos
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
