add_library(geom
    mat3.c "${pdf_SOURCE_DIR}/include/geom/mat3.h"
    vec2.c "${pdf_SOURCE_DIR}/include/geom/vec2.h"
    vec3.c "${pdf_SOURCE_DIR}/include/geom/vec3.h"
    rect.c "${pdf_SOURCE_DIR}/include/geom/rect.h")
target_include_directories(geom PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(geom PRIVATE pdf-logger $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
target_compile_features(geom PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(geom PRIVATE -fsanitize=address)
    target_link_options(geom PRIVATE -fsanitize=address)
endif()
