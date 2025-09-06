add_library(pdf-logger
    log.c)
target_include_directories(pdf-logger PUBLIC "${pdf_SOURCE_DIR}/include")
target_compile_features(pdf-logger PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(pdf-logger PRIVATE -fsanitize=address,undefined)
    target_link_options(pdf-logger PRIVATE -fsanitize=address,undefined)
endif()
