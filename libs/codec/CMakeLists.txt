add_library(codec
    src/zlib.c
    src/deflate.c
    src/adler32.c
    src/bitstream.c)
target_include_directories(codec PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(codec PUBLIC arena err)
target_link_libraries(codec PRIVATE pdf-logger pdf-test)
target_compile_features(codec PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(codec PRIVATE -fsanitize=address,undefined)
    target_link_options(codec PRIVATE -fsanitize=address,undefined)
endif()
