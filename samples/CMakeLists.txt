#
# Copyright (c) 2023 Ben Ashbaugh
#
# SPDX-License-Identifier: MIT
#

function(add_level_zero_sample)
    set(options TEST)
    set(one_value_args NUMBER TARGET VERSION CATEGORY)
    set(multi_value_args SOURCES KERNELS INCLUDES LIBS)
    cmake_parse_arguments(LEVEL_ZERO_SAMPLE
        "${options}" "${one_value_args}" "${multi_value_args}"
        ${ARGN}
    )

    if(NOT LEVEL_ZERO_SAMPLE_NUMBER)
        message(STATUS "No sample number specified for sample ${LEVEL_ZERO_SAMPLE_TARGET}, using 99.")
        set(LEVEL_ZERO_SAMPLE_NUMBER 99)
    endif()

    add_executable(${LEVEL_ZERO_SAMPLE_TARGET} ${LEVEL_ZERO_SAMPLE_SOURCES})

    target_include_directories(${LEVEL_ZERO_SAMPLE_TARGET} PRIVATE ${LevelZero_INCLUDE_DIR} ${LEVEL_ZERO_SAMPLE_INCLUDES})
    target_link_libraries(${LEVEL_ZERO_SAMPLE_TARGET} ${LevelZero_LIBRARIES} ${LEVEL_ZERO_SAMPLE_LIBS})

    if (WIN32)
        target_compile_definitions(${LEVEL_ZERO_SAMPLE_TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    set_target_properties(${LEVEL_ZERO_SAMPLE_TARGET} PROPERTIES FOLDER "Samples/${LEVEL_ZERO_SAMPLE_CATEGORY}/${LEVEL_ZERO_SAMPLE_NUMBER}_${LEVEL_ZERO_SAMPLE_TARGET}")

    if(CMAKE_CONFIGURATION_TYPES)
        set(LEVEL_ZERO_SAMPLE_CONFIGS ${CMAKE_CONFIGURATION_TYPES})
    else()
        set(LEVEL_ZERO_SAMPLE_CONFIGS ${CMAKE_BUILD_TYPE})
    endif()
    foreach(CONFIG ${LEVEL_ZERO_SAMPLE_CONFIGS})
        install(TARGETS ${LEVEL_ZERO_SAMPLE_TARGET} CONFIGURATIONS ${CONFIG} DESTINATION ${CONFIG})
        install(FILES ${LEVEL_ZERO_SAMPLE_KERNELS} CONFIGURATIONS ${CONFIG} DESTINATION ${CONFIG})
    endforeach()
    if(LEVEL_ZERO_SAMPLE_TEST)
        add_test(NAME ${LEVEL_ZERO_SAMPLE_TARGET} COMMAND ${LEVEL_ZERO_SAMPLE_TARGET})
    endif()
endfunction()

add_subdirectory( 00_enumlevelzero )
add_subdirectory( 00_extendeddevicequeries )
add_subdirectory( 01_lzinfo )
add_subdirectory( 02_hellosysman )
