cmake_minimum_required(VERSION 3.10.0)

project(radiotuner VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/radiotunerrx_v2.cpp
    ace/radiotunertx.cpp
    ace/countryregulations.cpp
    ace/countryregulations2.cpp
    ace/rxdataframequeue.cpp
)

set(PICO_EXTRA_LIBS
    pico_cyw43_arch_lwip_sys_freertos
    pico_time 
    pico_rand
)

set(MODULE_TARGET_LINK
    utils
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
