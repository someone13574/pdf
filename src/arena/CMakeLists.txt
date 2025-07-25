add_library(arena
    arena.c "${pdf_SOURCE_DIR}/include/arena/arena.h"
    "${pdf_SOURCE_DIR}/include/arena/darray_decl.h"
    "${pdf_SOURCE_DIR}/include/arena/darray_impl.h"
    "${pdf_SOURCE_DIR}/include/arena/dvec_decl.h"
    "${pdf_SOURCE_DIR}/include/arena/dvec_impl.h"
    darray_test.c dvec_test.c)
target_include_directories(arena PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(arena PRIVATE pdf-logger pdf-test)
target_compile_features(arena PUBLIC c_std_23)
target_compile_options(arena PRIVATE -fsanitize=address)
target_link_options(arena PRIVATE -fsanitize=address)
