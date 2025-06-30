set(HEADER_LIST
    "${pdf_SOURCE_DIR}/include/pdf.h"
    "${pdf_SOURCE_DIR}/include/log.h")

add_library(pdf pdf.c log.c ${HEADER_LIST})
target_include_directories(pdf PUBLIC ../include)
target_compile_features(pdf PUBLIC c_std_23)

source_group(
    TREE "${PROJECT_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${HEADER_LIST})
