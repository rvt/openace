cmake_minimum_required(VERSION 3.18)

project(
  wifiservice
  VERSION 0.0.0
  LANGUAGES CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)

gatas_add_module(
    SOURCE_FILES
        ace/src/wifiservice.cpp
        dnsserver/src/dnsserver.c
        dhcpserver/src/dhcpserver.c
    PICO_EXTRA_LIBS
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_mdns
)
