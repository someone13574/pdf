add_library(color
    src/cie.c
    src/conversion.c
    src/icc.c
    src/icc_types.c
    src/icc_cache.c
    src/icc_color.c
    src/icc_curve.c
    src/icc_lut.c
    src/icc_tags.c
    src/rgb.c)
target_include_directories(color PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(color PUBLIC geom parse-ctx)
target_link_libraries(color PRIVATE pdf-test)
target_compile_features(color PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(color PRIVATE -fsanitize=address,undefined)
    target_link_options(color PRIVATE -fsanitize=address,undefined)
endif()
