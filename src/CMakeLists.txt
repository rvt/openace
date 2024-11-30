cmake_minimum_required(VERSION 3.10)
project(MasterProject)

message("===============================================================================")
message("== Build and run unit tests                                                  ==")
message("===============================================================================")

# Add cmake modules
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/etl etlcpp)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/libcrc libcrc)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/libmodes libmodes)

# Add subdirectories for each project
add_subdirectory(lib/adsl/adsl_tests)
add_subdirectory(lib/adsbdecoder/adsbdecoder_tests)
add_subdirectory(lib/flarm/flarm_tests)
add_subdirectory(lib/core/core_tests)
add_subdirectory(lib/radiotuner/radiotuner_tests)
add_subdirectory(lib/aircrafttracker/aircrafttracker_tests)
