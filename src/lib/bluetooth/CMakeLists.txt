cmake_minimum_required(VERSION 3.5.0)

project(bluetooth VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)

set(MODULE_SOURCE_FILES
    ace/bluetooth.cpp
)
#ace/nordic_spp_service_server.c

set(MODULE_TARGET_LINK
    utils
)

set(PICO_EXTRA_LIBS
    pico_btstack_ble # BLE Bluetooth
    pico_btstack_classic # Classic Bluetooth
    pico_btstack_cyw43
    pico_cyw43_arch_lwip_sys_freertos
)


# Include the directory where the header was generated
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated/OpenAce_gatt_header)

include(${CMAKE_CURRENT_SOURCE_DIR}/../openace_module.cmake)

pico_btstack_make_gatt_header(OpenAce PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ace/openace_gatt.gatt")
