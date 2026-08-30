cmake_minimum_required(VERSION 3.18)
project(gps_tests)
include(FetchContent)

add_definitions(-DCATCH_CONFIG_NO_POSIX_SIGNALS)
add_definitions(-DGATAS_MAXIMUM_TCP_CLIENTS=4)
set(CMAKE_CXX_STANDARD 17)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.1)
FetchContent_MakeAvailable(Catch2)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../ace")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../core")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../mocks")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor/minmea")

if(${CMAKE_SOURCE_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor/etl etlcpp)
endif()

add_executable(staticgps_nmea_test
  ../../../vendor/minmea/minmea.c
  staticgps_nmea_test.cpp)

target_link_libraries(staticgps_nmea_test PRIVATE Catch2WithMain etl)

add_custom_command(
  TARGET staticgps_nmea_test
  COMMENT "Run StaticGPS NMEA tests"
  POST_BUILD
  COMMAND staticgps_nmea_test)
