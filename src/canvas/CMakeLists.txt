add_library(canvas
    canvas.c "${pdf_SOURCE_DIR}/include/canvas/canvas.h")
target_include_directories(canvas PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(canvas PUBLIC arena)
target_link_libraries(canvas PRIVATE pdf-logger)
target_compile_features(canvas PUBLIC c_std_23)
target_compile_options(canvas PRIVATE -fsanitize=address)
target_link_options(canvas PRIVATE -fsanitize=address)
