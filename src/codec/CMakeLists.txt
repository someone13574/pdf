add_library(codec
    zlib.c "${pdf_SOURCE_DIR}/include/codec/zlib.h"
    deflate.c deflate.h
    adler32.c adler32.h
    bitstream.c bitstream.h)
target_include_directories(codec PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(codec PUBLIC arena pdf-error)
target_link_libraries(codec PRIVATE pdf-logger pdf-test)
target_compile_features(codec PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(codec PRIVATE -fsanitize=address,undefined)
    target_link_options(codec PRIVATE -fsanitize=address,undefined)
endif()
