add_library(sfnt
    glyph.c glyph.h "${pdf_SOURCE_DIR}/include/sfnt/glyph.h"
    sfnt.c "${pdf_SOURCE_DIR}/include/sfnt/sfnt.h"
    types.c "${pdf_SOURCE_DIR}/include/sfnt/types.h"
    cmap.c cmap.h
    directory.c directory.h
    head.c head.h
    loca.c loca.h
    maxp.c maxp.h
    parser.c parser.h)
target_include_directories(sfnt PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(sfnt PUBLIC arena canvas)
target_link_libraries(sfnt PRIVATE pdf-error pdf-logger pdf-test)
target_compile_features(sfnt PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(sfnt PRIVATE -fsanitize=address)
    target_link_options(sfnt PRIVATE -fsanitize=address)
endif()
