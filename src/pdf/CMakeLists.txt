add_library(pdf
    catalog.c
    stream/stream_dict.c
    stream/filters.c
    stream/ascii.c
    content_stream/op.c
    content_stream/stream.c
    content_stream/operator.c
    resources/resources.c
    resources/font.c
    deserde_types.c
    object.c
    page.c
    pdf.c
    trailer.c
    types.c
    ctx.c
    deserialize.c
    test_helpers.c
    xref.c)
target_include_directories(pdf PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(pdf PUBLIC arena pdf-error)
target_link_libraries(pdf PRIVATE codec pdf-logger pdf-test)
target_compile_features(pdf PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(pdf PRIVATE -fsanitize=address,undefined)
    target_link_options(pdf PRIVATE -fsanitize=address,undefined)
endif()
