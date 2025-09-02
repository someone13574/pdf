add_library(canvas
    canvas.c canvas.h "${pdf_SOURCE_DIR}/include/canvas/canvas.h"
    path_builder.c path_builder.h "${pdf_SOURCE_DIR}/include/canvas/path_builder.h"
    dcel_builder.c "${pdf_SOURCE_DIR}/include/canvas/dcel_builder.h"
    dcel.c dcel.h
    raster_canvas.c raster_canvas.h
    scalable_canvas.c scalable_canvas.h)
target_include_directories(canvas PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(canvas PUBLIC arena)
target_link_libraries(canvas PRIVATE pdf-logger $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
target_compile_features(canvas PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(canvas PRIVATE -fsanitize=address,undefined)
    target_link_options(canvas PRIVATE -fsanitize=address,undefined)
endif()
