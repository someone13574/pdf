add_library(arena
    src/arena.c
    src/common.c
    src/string.c
    src/darray_test.c src/dvec_test.c src/dlinked_test.c)
target_include_directories(arena PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(arena PRIVATE logger pdf-test)
target_compile_features(arena PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(arena PRIVATE -fsanitize=address,undefined)
    target_link_options(arena PRIVATE -fsanitize=address,undefined)
endif()
