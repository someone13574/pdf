add_library(geom
    mat3.c
    vec2.c
    vec3.c
    rect.c)
target_include_directories(geom PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(geom PRIVATE pdf-logger $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
target_compile_features(geom PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(geom PRIVATE -fsanitize=address,undefined)
    target_link_options(geom PRIVATE -fsanitize=address,undefined)
endif()
