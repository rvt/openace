cmake_minimum_required(VERSION 3.10.0)

project(adslace VERSION 0.0.0 LANGUAGES CXX)
set(MSG_PREFIX "${PROJECT_NAME} |")

set(MODULE_SOURCE_FILES
        ace/src/adslace.cpp
)

set(MODULE_TARGET_LINK
    utils
    adsl
)

set(PICO_EXTRA_LIBS
    adsl::adsl
)


include(${CMAKE_CURRENT_SOURCE_DIR}/../gatas_module.cmake)
