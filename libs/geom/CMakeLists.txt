add_library(geom
    src/mat3.c
    src/vec2.c
    src/vec3.c
    src/rect.c)
target_include_directories(geom PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(geom PRIVATE pdf-logger $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
target_compile_features(geom PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(geom PRIVATE -fsanitize=address,undefined)
    target_link_options(geom PRIVATE -fsanitize=address,undefined)
endif()
