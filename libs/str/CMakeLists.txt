add_library(str
    src/alloc_str.c
    src/ref.c)
target_include_directories(str PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(str PUBLIC arena)
target_link_libraries(str PRIVATE logger)
target_compile_features(str PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(str PRIVATE -fsanitize=address,undefined)
    target_link_options(str PRIVATE -fsanitize=address,undefined)
endif()
