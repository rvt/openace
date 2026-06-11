cmake_minimum_required(VERSION 3.18)

project(sx1262 VERSION 0.0.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)

gatas_add_module(
    SOURCE_FILES
        ace/src/sx1262.cpp
        ace/src/sx1262_hal.cpp
        ace/src/rxdataframequeue.cpp

        # Driver from Semtech
        driver/src/sx126x.c
        driver/src/sx126x_lr_fhss.c
        driver/src/sx126x_driver_version.c
        driver/src/lr_fhss_mac.c
    TARGET_LINK_LIBS
        utils
    PICO_EXTRA_LIBS
        hardware_spi
)
