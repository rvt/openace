cmake_minimum_required(VERSION 3.18)

project(pioserial VERSION 0.0.0 LANGUAGES CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)

gatas_add_module(
    SOURCE_FILES
        ace/src/pioserial.cpp
    PIO_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/ace/uart_rx.pio
        ${CMAKE_CURRENT_SOURCE_DIR}/ace/uart_tx.pio
    TARGET_LINK_LIBS
        utils
    PICO_EXTRA_LIBS
        hardware_gpio
        hardware_irq
        hardware_uart
        hardware_pio
)
