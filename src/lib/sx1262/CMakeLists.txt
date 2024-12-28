cmake_minimum_required(VERSION 3.5.0)

project(sx1262 VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/sx1262.cpp
    ace/sx1262_hal.cpp

    # Driver from Semtech
    driver/src/sx126x.c 
    driver/src/sx126x_lr_fhss.c
    driver/src/sx126x_driver_version.c
    driver/src/lr_fhss_mac.c
)

set(MODULE_TARGET_LINK
    utils
)

set(PICO_EXTRA_LIBS
    hardware_spi
    hardware_pio
    pico_cyw43_arch_lwip_sys_freertos
)


include(${CMAKE_CURRENT_SOURCE_DIR}/../openace_module.cmake)
