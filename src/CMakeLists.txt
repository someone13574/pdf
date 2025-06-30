set(HEADER_LIST
    "${pdf_SOURCE_DIR}/include/pdf.h"
    "${pdf_SOURCE_DIR}/include/log.h")
set(SOURCE_FILE_LIST pdf.c log.c ${HEADER_LIST})

add_library(pdf ${SOURCE_FILE_LIST})
target_include_directories(pdf PUBLIC ../include)
target_compile_features(pdf PUBLIC c_std_23)

add_executable(pdf-test test.c test.h ${SOURCE_FILE_LIST})
target_include_directories(pdf-test PUBLIC ../include)
target_compile_features(pdf-test PUBLIC c_std_23)

source_group(
    TREE "${PROJECT_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${HEADER_LIST})
