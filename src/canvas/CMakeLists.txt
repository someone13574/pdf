add_library(canvas
    canvas.c "${pdf_SOURCE_DIR}/include/canvas/canvas.h"
    path_builder.c "${pdf_SOURCE_DIR}/include/canvas/path_builder.h"
    dcel.c dcel.h)
target_include_directories(canvas PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(canvas PUBLIC arena)
target_link_libraries(canvas PRIVATE pdf-logger m)
target_compile_features(canvas PUBLIC c_std_23)
target_compile_options(canvas PRIVATE -fsanitize=address)
target_link_options(canvas PRIVATE -fsanitize=address)
