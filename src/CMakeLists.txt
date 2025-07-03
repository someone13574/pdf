set(HEADER_LIST
    "${pdf_SOURCE_DIR}/include/pdf.h"
    "${pdf_SOURCE_DIR}/include/log.h")
set(SOURCE_FILE_LIST
    pdf.c
    log.c
    ctx.c
    ctx.h
    parse.c
    parse.h
    xref.c
    xref.h
    ${HEADER_LIST})

add_library(pdf ${SOURCE_FILE_LIST})
target_include_directories(pdf PUBLIC ../include)
target_compile_features(pdf PUBLIC c_std_23)
target_compile_definitions(pdf PRIVATE DEBUG TEST)
target_compile_options(pdf PRIVATE -fsanitize=address)
target_link_options(pdf PRIVATE -fsanitize=address)

add_executable(pdf-test test.c test.h ${SOURCE_FILE_LIST})
target_include_directories(pdf-test PUBLIC ../include)
target_compile_features(pdf-test PUBLIC c_std_23)
target_compile_definitions(pdf-test PRIVATE DEBUG TEST)
target_compile_options(pdf-test PRIVATE -fsanitize=address)
target_link_options(pdf-test PRIVATE -fsanitize=address)

source_group(
    TREE "${PROJECT_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${HEADER_LIST})
