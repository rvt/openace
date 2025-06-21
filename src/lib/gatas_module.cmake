# common_module.cmake
set(MSG_PREFIX "${PROJECT_NAME} |")
message("### Including ${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}")

option(BUILD_TESTS "Build unit tests" OFF)
option(NO_STL "No STL" OFF)

# Create the library
add_library(${PROJECT_NAME} STATIC ${MODULE_SOURCE_FILES})

# Include directories
target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
)

# Set compile options for each file of GaTas
set_source_files_properties(${MODULE_SOURCE_FILES} PROPERTIES
    COMPILE_FLAGS "-Wall -Wextra -Werror"
)

foreach(PIO_FILENAME ${MODULE_PIO_FILES})
    pico_generate_pio_header(${PROJECT_NAME} ${PIO_FILENAME})
endforeach()

# Since core our own project that uses this, we don't add it when it'c core
if (NOT "${PROJECT_NAME}" STREQUAL "core")
    set(TARGET_CORE core)
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    PRIVATE
    FreeRTOS-Kernel-Heap4
    pico_stdlib

    ${PICO_EXTRA_LIBS}
    etl::etl
    ${TARGET_CORE}
    ${MODULE_TARGET_LINK}
)
