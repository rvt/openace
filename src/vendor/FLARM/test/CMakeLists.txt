cmake_minimum_required(VERSION 3.18)
project(tests)
include(FetchContent)
include(CTest)
enable_testing()
message(STATUS "Building tests.")

option(ENABLE_COVERAGE "Enable code coverage" OFF)

add_definitions(-DCATCH_CONFIG_NO_POSIX_SIGNALS)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# Pull ETL framework
FetchContent_Declare(
  etl
  GIT_REPOSITORY https://github.com/ETLCPP/etl
  GIT_TAG        20.39.4
)

FetchContent_GetProperties(etl)
if(NOT etl_POPULATED)
    FetchContent_Populate(etl)
    add_subdirectory(${etl_SOURCE_DIR} ${etl_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()


# Pull in the Catch2 framework.
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.1)
FetchContent_MakeAvailable(Catch2)

# These examples use the standard separate compilation
set(SOURCES_IDIOMATIC_EXAMPLES # Tests
  flarm2024packet_test.cpp
)

string(REPLACE ".cpp" "" BASENAMES_IDIOMATIC_EXAMPLES
               "${SOURCES_IDIOMATIC_EXAMPLES}")
set(TARGETS_IDIOMATIC_EXAMPLES ${BASENAMES_IDIOMATIC_EXAMPLES})


if(ENABLE_COVERAGE)
  set(CMAKE_CXX_FLAGS "-O0 -coverage")

  find_program(LCOV lcov REQUIRED)
  find_program(GENHTML genhtml REQUIRED)

  foreach(name ${TARGETS_IDIOMATIC_EXAMPLES})
    add_custom_target("${name}_coverage"
    COMMAND ${LCOV} --ignore-errors source,inconsistent,format,format,range --directory . --capture --initial --output-file ${name}_base.info
    COMMAND ${name}
    COMMAND ${LCOV} --ignore-errors source,inconsistent,format,format,range --directory . --capture --output-file ${name}.info 
    COMMAND ${LCOV} --ignore-errors source,inconsistent,format,unused,range --remove ${name}.info '/usr/*' '*/etl-src/*' 'catch2/*' --output-file ${name}.filtered.info
    COMMAND ${GENHTML} --ignore-errors inconsistent,corrupt,category,range ${name}.filtered.info --out ${name}_coverage

    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating code coverage report.")
  endforeach()
endif()

if(NOT ENABLE_COVERAGE)
foreach(name ${TARGETS_IDIOMATIC_EXAMPLES})
  add_executable(${name} ${name}.cpp ../include/flarm/lib_crc.cpp)

  target_link_libraries(
      ${name} 
      Catch2::Catch2WithMain
      flarm::flarm
      etl)

  add_test(NAME ${name} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${name})
endforeach()
endif()

list(APPEND CATCH_WARNING_TARGETS ${ALL_EXAMPLE_TARGETS})
set(CATCH_WARNING_TARGETS
    ${CATCH_WARNING_TARGETS}
    PARENT_SCOPE)
