add_library(canvas
    src/canvas.c
    src/path_builder.c
    src/dcel_builder.c
    src/dcel.c
    src/raster_canvas.c
    src/scalable_canvas.c)
target_include_directories(canvas PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(canvas PUBLIC arena geom)
target_link_libraries(canvas PRIVATE logger $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
target_compile_features(canvas PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(canvas PRIVATE -fsanitize=address,undefined)
    target_link_options(canvas PRIVATE -fsanitize=address,undefined)
endif()
