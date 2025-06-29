cmake_minimum_required(VERSION 3.24)
project(pdf LANGUAGES C)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    add_compile_options(
        -Weverything
        -Werror
        -pedantic
        -Wno-error=unused-function
        -Wno-error=unused-parameter
        -Wno-error=unused-variable
        -Wno-error=unreachable-code
    )
endif()

add_subdirectory(src)
add_subdirectory(cli)
