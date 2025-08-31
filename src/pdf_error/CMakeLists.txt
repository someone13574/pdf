add_library(pdf-error
    error.c "${pdf_SOURCE_DIR}/include/pdf_error/error.h")
target_include_directories(pdf-error PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(pdf-error PRIVATE pdf-logger)
target_compile_features(pdf-error PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(pdf-error PRIVATE -fsanitize=address)
    target_link_options(pdf-error PRIVATE -fsanitize=address)
endif()
