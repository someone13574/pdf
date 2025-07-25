add_library(pdf-logger
    log.c "${pdf_SOURCE_DIR}/include/logger/log.h"
    "${pdf_SOURCE_DIR}/include/logger/log_groups.h")
target_include_directories(pdf-logger PUBLIC "${pdf_SOURCE_DIR}/include")
target_compile_features(pdf-logger PUBLIC c_std_23)
target_compile_options(pdf-logger PRIVATE -fsanitize=address)
target_link_options(pdf-logger PRIVATE -fsanitize=address)
