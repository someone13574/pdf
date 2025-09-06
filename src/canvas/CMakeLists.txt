add_library(canvas
    canvas.c
    path_builder.c
    dcel_builder.c
    dcel.c
    raster_canvas.c
    scalable_canvas.c)
target_include_directories(canvas PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(canvas PUBLIC arena)
target_link_libraries(canvas PRIVATE pdf-logger $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
target_compile_features(canvas PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(canvas PRIVATE -fsanitize=address,undefined)
    target_link_options(canvas PRIVATE -fsanitize=address,undefined)
endif()
