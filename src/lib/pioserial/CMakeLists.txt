cmake_minimum_required(VERSION 3.5.0)

project(pioserial VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
    ace/pioserial.cpp
)

set(MODULE_PIO_FILES
   ${CMAKE_CURRENT_SOURCE_DIR}/ace/uart_rx.pio
   ${CMAKE_CURRENT_SOURCE_DIR}/ace/uart_tx.pio
)

set(MODULE_TARGET_LINK
    utils
)

set(PICO_EXTRA_LIBS
    hardware_gpio
    hardware_irq
    hardware_uart
    hardware_pio
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../openace_module.cmake)
