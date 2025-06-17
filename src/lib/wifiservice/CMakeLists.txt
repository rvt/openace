cmake_minimum_required(VERSION 3.10.0)

project(
  wifiservice
  VERSION 0.0.0
  LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES ace/wifiservice.cpp dnsserver/dnsserver.c
                        dhcpserver/dhcpserver.c)

set(PICO_EXTRA_LIBS pico_cyw43_arch_lwip_sys_freertos pico_lwip_mdns)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
