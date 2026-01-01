add_library(sfnt
    src/glyph.c
    src/sfnt.c
    src/types.c
    src/cmap.c
    src/directory.c
    src/head.c
    src/loca.c
    src/maxp.c
    src/hmtx.c
    src/hhea.c
    src/parser.c)
target_include_directories(sfnt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(sfnt PUBLIC arena canvas geom)
target_link_libraries(sfnt PRIVATE err pdf-logger pdf-test)
target_compile_features(sfnt PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(sfnt PRIVATE -fsanitize=address,undefined)
    target_link_options(sfnt PRIVATE -fsanitize=address,undefined)
endif()
