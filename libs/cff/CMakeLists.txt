add_library(cff
    src/cff.c
    src/charsets.c
    src/header.c
    src/index.c
    src/parser.c
    src/top_dict.c
    src/types.c)
target_include_directories(cff PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(cff PUBLIC arena pdf-error)
target_link_libraries(cff PRIVATE geom pdf-logger pdf-test)
target_compile_features(cff PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(cff PRIVATE -fsanitize=address,undefined)
    target_link_options(cff PRIVATE -fsanitize=address,undefined)
endif()
