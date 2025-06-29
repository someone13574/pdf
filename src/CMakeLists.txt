set(HEADER_LIST "${pdf_SOURCE_DIR}/include/pdf.h")

add_library(pdf pdf.c ${HEADER_LIST})
target_include_directories(pdf PUBLIC ../include)

source_group(
    TREE "${PROJECT_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${HEADER_LIST})
