add_library(sfnt
    glyph.c
    sfnt.c
    types.c
    cmap.c
    directory.c
    head.c
    loca.c
    maxp.c
    hmtx.c
    hhea.c
    parser.c)
target_include_directories(sfnt PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(sfnt PUBLIC arena canvas geom)
target_link_libraries(sfnt PRIVATE pdf-error pdf-logger pdf-test)
target_compile_features(sfnt PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(sfnt PRIVATE -fsanitize=address,undefined)
    target_link_options(sfnt PRIVATE -fsanitize=address,undefined)
endif()
