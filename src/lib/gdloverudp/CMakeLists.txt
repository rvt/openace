cmake_minimum_required(VERSION 3.18)

project(gdloverudp VERSION 0.0.0 LANGUAGES CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)

gatas_add_module(
    SOURCE_FILES
        ace/src/gdloverudp.cpp
    TARGET_LINK_LIBS
        utils
        TinyJson
    PICO_EXTRA_LIBS
        pico_cyw43_arch_lwip_sys_freertos
        utils
)
