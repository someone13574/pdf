add_library(pdf
    src/catalog.c
    src/color_space.c
    src/stream/stream_dict.c
    src/stream/filters.c
    src/stream/ascii.c
    src/content_stream/operation.c
    src/content_stream/stream.c
    src/content_stream/operator.c
    src/resources.c
    src/fonts/agl.c
    src/fonts/cid_to_gid_map.c
    src/fonts/font.c
    src/fonts/font_descriptor.c
    src/fonts/font_widths.c
    src/fonts/cmap.c
    src/fonts/cmap_paths.c
    src/fonts/encoding.c
    src/object.c
    src/page.c
    src/pdf.c
    src/trailer.c
    src/types.c
    src/ctx.c
    src/deserialize.c
    src/test_helpers.c
    src/xobject.c
    src/xref.c)
target_include_directories(pdf PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pdf PUBLIC arena geom pdf-error)
target_link_libraries(pdf PRIVATE codec pdf-logger pdf-test postscript)
target_compile_features(pdf PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(pdf PRIVATE -fsanitize=address,undefined)
    target_link_options(pdf PRIVATE -fsanitize=address,undefined)
endif()
