cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(PICO_CXX_ENABLE_EXCEPTIONS 0)
set(PICO_CXX_ENABLE_RTTI 0)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)
set(DEFAULT_CONFIG_HEADER "${CMAKE_CURRENT_LIST_DIR}/generated/default_config.hpp")
set(OPTIMIZE_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/external/optimizejson.py
                    gatas_default_config.json ${DEFAULT_CONFIG_HEADER})
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico_w)
endif()
if(NOT DEFINED PICO_PLATFORM)
    set(PICO_PLATFORM rp2040)
endif()

execute_process(
  COMMAND ${OPTIMIZE_SCRIPT}
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} ECHO_OUTPUT_VARIABLE ECHO_ERROR_VARIABLE)

# All defines go in config.hpp that should not be configurable for the board
# itself. Default configurations could go here if that makes sense
set(CONFIG_HEADER "${CMAKE_CURRENT_LIST_DIR}/generated/config.hpp")
configure_file(config.hpp.in ${CONFIG_HEADER} @ONLY)
add_definitions(-include ${CONFIG_HEADER})

# Add buildtime timestamp Move this to a dedicated 'status' module to reduce
# build times
# Get the exact tag if the commit is tagged
execute_process(
    COMMAND git describe --exact-match --tags
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    RESULT_VARIABLE TAG_RESULT
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT TAG_RESULT EQUAL 0)
    set(GIT_TAG "UNKNOWN_TAG")
endif()
string(TIMESTAMP BUILD_TIMESTAMP_EPOCH "%s" UTC)

set(BUILD_TIME_HEADER "${CMAKE_CURRENT_LIST_DIR}/generated/build_time.hpp")
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%SZ" UTC)
file(WRITE "${BUILD_TIME_HEADER}"
    "#pragma once\n"
    "#define BUILD_TIMESTAMP \"${BUILD_TIMESTAMP}\"\n"
    "#define BUILD_TIMESTAMP_EPOCH ${BUILD_TIMESTAMP_EPOCH}\n"
    "#define BUILD_GIT_TAG \"${GIT_TAG}\"\n"
)
#add_definitions(-include ${BUILD_TIME_HEADER})

# Create a custom command and target to ensure the header is generated
add_custom_command(
  OUTPUT "${BUILD_TIME_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E touch "${BUILD_TIME_HEADER}"
  DEPENDS "${BUILD_TIME_HEADER}")

# pico_sdk_import.cmake is a single file copied from this SDK note: this must
# happen before project() include(cmake/pico_sdk_import.cmake)7s
include(cmake/pico_sdk_import.cmake)
include(cmake/FreeRTOS_Kernel_import.cmake)

if(PICO_SDK_VERSION_STRING VERSION_LESS "2.1.0")
  message(
    FATAL_ERROR
      "Raspberry Pi Pico SDK version 2.1.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}"
  )
endif()

set(OUTPUT_NAME "GATAS_${PICO_PLATFORM}")
project(${OUTPUT_NAME} C CXX ASM)

# Allow user to override this option from command line: -DGATAS_UART_OVER_USB=1
option(GATAS_UART_OVER_USB "Enable UART over USB (1) or physical UART (0)" "")

# Set default based on build type if not provided
if(GATAS_UART_OVER_USB STREQUAL "")
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(GATAS_UART_OVER_USB 1 CACHE BOOL "Enable UART over USB by default in Release builds" FORCE)
  else()
    set(GATAS_UART_OVER_USB 0 CACHE BOOL "Enable UART over USB by default in Debug builds" FORCE)
  endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(
    -O2
    -Wall
    -fsingle-precision-constant
    -ffast-math
    -ffunction-sections
    -fdata-sections
    )
else()
  add_compile_options(
    -g 
    -O0
    -Wall
    -fsingle-precision-constant
    -ffast-math
    -ffunction-sections
    -fdata-sections
    )
    endif()

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
                            PROPERTIES COMPILE_FLAGS "-Wall -Wextra -Werror -Wvexing-parse
")

pico_sdk_init()

add_executable(${PROJECT_NAME} main.cpp IdleMemory.c)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor vendorLib)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lib globalLib)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR} 
  ${CMAKE_CURRENT_LIST_DIR}/../lib/include)


target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE FreeRTOS-Kernel-Heap4
          pico_btstack_ble
          pico_btstack_classic
          pico_btstack_cyw43
          pico_cyw43_arch_lwip_sys_freertos
          pico_stdlib
          hardware_gpio
          hardware_spi
#          hardware_rtc
          hardware_irq
          hardware_uart
          hardware_pio
          # Vendor
          GDL90
          minmea
          etl::etl
          fanet::fanet
          adsl::adsl
          ArduinoJson
          # /lib
          core
          pioserial
          serialadsb
          config
          gps
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
          adslace
          flarmgatas
          fanetace
          ogn
          radiotuner
          gdl90service
          gdloverudp
          utils
          aircrafttracker
          dataport
          airconnect
          gatasconnect
          bluetooth
          idle
          )

target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE FREE_RTOS_KERNEL_SMP=1
          portSUPPORT_SMP=1
          GATAS_UART_OVER_USB=${GATAS_UART_OVER_USB}
          PICO_USE_STACK_GUARDS=1)

# Enable debugging over PICO Debugger
if(GATAS_UART_OVER_USB)
  pico_enable_stdio_usb(${PROJECT_NAME} 1)
  pico_enable_stdio_uart(${PROJECT_NAME} 0)
else()
  pico_enable_stdio_usb(${PROJECT_NAME} 0)
  pico_enable_stdio_uart(${PROJECT_NAME} 1)
endif()

# create map/bin/hex/uf2 file in addition to ELF. -> rp2_common.cmake
pico_add_extra_outputs(${PROJECT_NAME})
