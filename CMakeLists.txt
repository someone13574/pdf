cmake_minimum_required(VERSION 3.24)
project(pdf LANGUAGES C)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
    add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

    add_compile_options(
        -Wall
        -Wdeprecated
        -Wextra
        -Wpedantic
        -pedantic
        -Wcast-align
        -Wdouble-promotion
        -Wimplicit-fallthrough
        -Wmisleading-indentation
        -Wshadow
        -Wconversion
        -Wmissing-field-initializers
        -Werror
        -Wno-error=unused-function
        -Wno-error=unused-parameter
        -Wno-error=unused-variable
        -Wno-error=unreachable-code
    )
endif()

add_subdirectory(src)
add_subdirectory(cli)
