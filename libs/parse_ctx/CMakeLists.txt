add_library(parse-ctx
    src/ctx.c)
target_include_directories(
    parse-ctx PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(parse-ctx PUBLIC arena err logger str)
target_link_libraries(parse-ctx PRIVATE pdf-test)
target_compile_features(parse-ctx PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(parse-ctx PRIVATE -fsanitize=address,undefined)
    target_link_options(parse-ctx PRIVATE -fsanitize=address,undefined)
endif()
