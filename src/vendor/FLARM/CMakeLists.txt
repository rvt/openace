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

add_library(${PROJECT_NAME} INTERFACE)
# This allows users which use the add_subdirectory or FetchContent
# to use the same target as users which use find_package
add_library(flarm::flarm ALIAS ${PROJECT_NAME})

include(GNUInstallDirs)

target_include_directories(${PROJECT_NAME} ${INCLUDE_SPECIFIER} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        )

target_link_libraries(${PROJECT_NAME} INTERFACE)

# only install if top level project
if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
    # Steps here based on excellent guide: https://dominikberner.ch/cmake-interface-lib/
    # Which also details all steps
    include(CMakePackageConfigHelpers)
    install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    if(FLARM_LIB_VERSION)
        # Generate the package configuration files using CMake provided macros
        write_basic_package_version_file(
            "${PROJECT_NAME}ConfigVersion.cmake"
            COMPATIBILITY SameMajorVersion
            ARCH_INDEPENDENT
        )
    endif()
    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION
        ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

    # Install target file, then package configuration files, and finally the headers
    install(EXPORT ${PROJECT_NAME}Targets
        FILE ${PROJECT_NAME}Targets.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)
    install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/flarm DESTINATION include)

endif()

if (BUILD_TESTS)
    add_subdirectory(test)
endif()
