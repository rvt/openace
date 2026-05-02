function(gatas_add_module)
    set(options)
    set(oneValueArgs MESSAGE_PREFIX)
    set(multiValueArgs SOURCE_FILES TARGET_LINK_LIBS PICO_EXTRA_LIBS PIO_FILES)
    cmake_parse_arguments(GAM "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGV})

    if(NOT GAM_MESSAGE_PREFIX)
        set(GAM_MESSAGE_PREFIX "${PROJECT_NAME} | ")
    endif()

    message("${GAM_MESSAGE_PREFIX}Including ${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}")

    add_library(${PROJECT_NAME} STATIC ${GAM_SOURCE_FILES})

    target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
        PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    )

    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            gatas_build_options
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        SHOW_MESSAGE=1
    )

    set(MODULE_C_SOURCES)
    set(MODULE_CXX_SOURCES)
    foreach(MODULE_SOURCE_FILE IN LISTS GAM_SOURCE_FILES)
        get_filename_component(MODULE_SOURCE_EXT "${MODULE_SOURCE_FILE}" LAST_EXT)
        if(MODULE_SOURCE_EXT STREQUAL ".cpp" OR MODULE_SOURCE_EXT STREQUAL ".cxx" OR MODULE_SOURCE_EXT STREQUAL ".cc" OR MODULE_SOURCE_EXT STREQUAL ".C")
            list(APPEND MODULE_CXX_SOURCES "${MODULE_SOURCE_FILE}")
        elseif(MODULE_SOURCE_EXT STREQUAL ".c")
            list(APPEND MODULE_C_SOURCES "${MODULE_SOURCE_FILE}")
        endif()
    endforeach()

    if(MODULE_C_SOURCES)
        set_source_files_properties(${MODULE_C_SOURCES} PROPERTIES
            COMPILE_FLAGS "-Wall -Wextra -Werror"
        )
    endif()

    if(MODULE_CXX_SOURCES)
        set_source_files_properties(${MODULE_CXX_SOURCES} PROPERTIES
            COMPILE_FLAGS "-Wall -Wextra -Werror -Wvexing-parse"
        )
    endif()

    foreach(PIO_FILENAME IN LISTS GAM_PIO_FILES)
        pico_generate_pio_header(${PROJECT_NAME} ${PIO_FILENAME})
    endforeach()

    set(TARGET_CORE)
    if (NOT "${PROJECT_NAME}" STREQUAL "core")
        set(TARGET_CORE core)
    endif()

    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            FreeRTOS-Kernel-Heap4
            pico_stdlib
            ${GAM_PICO_EXTRA_LIBS}
            etl::etl
            ${TARGET_CORE}
            ${GAM_TARGET_LINK_LIBS}
    )
endfunction()
