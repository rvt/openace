cmake_minimum_required(VERSION 3.13)

set(OPTMIZE_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/external/optimizejson.py
                   openace_default_config.json default_config.hpp)
execute_process(
  COMMAND ${OPTMIZE_SCRIPT}
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} ECHO_OUTPUT_VARIABLE
                    ECHO_ERROR_VARIABLE)

# All defines go in config.hpp that should not be configurable for the board
# itself. Default configurations could go here if that makes sense
set(CONFIG_HEADER "${CMAKE_BINARY_DIR}/config.hpp")
configure_file(config.hpp.in ${CONFIG_HEADER} @ONLY)
add_definitions(-include ${CONFIG_HEADER})

# Add buildtime timestamp Move this to a dedicated 'status' module to reduce
# build times
set(BUILD_TIME_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/build_time.hpp")
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%SZ" UTC)
file(WRITE "${BUILD_TIME_HEADER}"
     "#pragma once\n" "#define BUILD_TIMESTAMP \"${BUILD_TIMESTAMP}\"\n")
# Create a custom command and target to ensure the header is generated
add_custom_command(
  OUTPUT "${BUILD_TIME_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E touch "${BUILD_TIME_HEADER}"
  DEPENDS "${BUILD_TIME_HEADER}")

set(PICO_BOARD pico_w)

# pico_sdk_import.cmake is a single file copied from this SDK note: this must
# happen before project() include(cmake/pico_sdk_import.cmake)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
include(cmake/FreeRTOS_Kernel_import.cmake)

project(OpenAce C CXX ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Enable this to use picotool, and beable to receive UART data over picotool
set(OPENACE_UART_OVER_USB 0)

add_compile_options(-Wall -fsingle-precision-constant -ffast-math)
add_link_options(-Wl,--print-memory-usage)

# Collect all .c/cpp/hpp/h files from the specified directory and its
# subdirectories So we can set the correct compile flag for them
file(
  GLOB_RECURSE
  C_SOURCE_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib/**/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib/**/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib/**/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib/**/*.cpp")

foreach(file ${C_SOURCE_FILES})
  get_filename_component(dir ${file} DIRECTORY)
  if("${dir}" MATCHES ".*/build(/.*)?")
    list(REMOVE_ITEM C_SOURCE_FILES ${file})
  endif()
endforeach()

# message(${C_SOURCE_FILES})

# Set for these files specific compile options
set_source_files_properties(${C_SOURCE_FILES}
                            PROPERTIES COMPILE_FLAGS "-Wall -Wextra -Werror")

# target_compile_definitions(TARGET PRIVATE PICO_DEFAULT_UART_TX_PIN=16
# PICO_DEFAULT_UART_RX_PIN=17 )
pico_sdk_init()

add_executable(${PROJECT_NAME} main.cpp IdleMemory.c)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor vendorLib)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lib globalLib)

target_include_directories(OpenAce PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE FreeRTOS-Kernel-Heap4
          pico_cyw43_arch_lwip_sys_freertos
          pico_stdlib
          hardware_spi
          hardware_rtc
          pioserial
          # Vendor
          GDL90
          minmea
          etl::etl
          ArduinoJson
          # /lib
          core
          tcpclient
          serialadsb
          config
          ubloxm8n
          adsbdecoder
          libmodes
          dump1090client
          wifiservice
          webserver
          gpsdecoder
          rtc
          acespi
          bmp280
          sx1262
          adsl
          flarm
          ogn
          radiotuner
          gdl90service
          gdloverudp
          utils
          aircrafttracker)

target_compile_definitions(
  OpenAce
  PRIVATE FREE_RTOS_KERNEL_SMP=1 portSUPPORT_SMP=1
          OPENACE_UART_OVER_USB=${OPENACE_UART_OVER_USB}
          PICO_USE_STACK_GUARDS=1)
          
# Enable debugging over PICO Debugger
if(OPENACE_UART_OVER_USB)
  pico_enable_stdio_usb(OpenAce 1)
  pico_enable_stdio_uart(OpenAce 0)
else()
  pico_enable_stdio_usb(OpenAce 0)
  pico_enable_stdio_uart(OpenAce 1)
endif()

# create map/bin/hex/uf2 file in addition to ELF. -> rp2_common.cmake
pico_add_extra_outputs(OpenAce)
