#######################################################################
# FLARM
#######################################################################
cmake_minimum_required(VERSION 3.10.0)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/helpers.cmake)

set(MSG_PREFIX "flarm |")
determine_version_with_git(${GIT_DIR_LOOKUP_POLICY})
if(NOT FLARM_LIB_VERSION)
    determine_version_with_file("version.txt")
endif()

project(flarm VERSION ${FLARM_LIB_VERSION} LANGUAGES CXX)

option(BUILD_TESTS "Build unit tests" OFF)
option(NO_STL "No STL" OFF)
# There is a bug on old gcc versions for some targets that causes all system headers
# to be implicitly wrapped with 'extern "C"'
# Users can add set(NO_SYSTEM_INCLUDE ON) to their top level CMakeLists.txt to work around this.
option(NO_SYSTEM_INCLUDE "Do not include with -isystem" OFF)
if (NO_SYSTEM_INCLUDE)
    set(INCLUDE_SPECIFIER "")
else()
    set(INCLUDE_SPECIFIER "SYSTEM")
endif()

# Library source files
file(GLOB_RECURSE FLARM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/flarm/*.cpp
)

# This allows users which use the add_subdirectory or FetchContent
# to use the same target as users which use find_package
#add_library(${PROJECT_NAME} INTERFACE)
#add_library(flarm::flarm ALIAS ${PROJECT_NAME})
add_library(${PROJECT_NAME} STATIC ${FLARM_SOURCES})

include(GNUInstallDirs)

target_include_directories(${PROJECT_NAME} ${INCLUDE_SPECIFIER} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        )

target_link_libraries(${PROJECT_NAME} INTERFACE)

if (BUILD_TESTS)
    add_subdirectory(test)
endif()
