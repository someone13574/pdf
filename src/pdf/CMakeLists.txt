add_library(pdf
    catalog.c "${pdf_SOURCE_DIR}/include/pdf/catalog.h"
    content_stream/op.c content_stream/op.h
    "${pdf_SOURCE_DIR}/include/pdf/content_op.h"
    content_stream/stream.c "${pdf_SOURCE_DIR}/include/pdf/content_stream.h"
    deserde_types.c "${pdf_SOURCE_DIR}/include/pdf/deserde_types.h"
    object.c "${pdf_SOURCE_DIR}/include/pdf/object.h"
    page.c "${pdf_SOURCE_DIR}/include/pdf/page.h"
    pdf.c
    "${pdf_SOURCE_DIR}/include/pdf/resolver.h" "${pdf_SOURCE_DIR}/include/pdf/pdf.h"
    trailer.c "${pdf_SOURCE_DIR}/include/pdf/trailer.h"
    types.c "${pdf_SOURCE_DIR}/include/pdf/types.h"
    content_stream/operator.c content_stream/operator.h
    ctx.c ctx.h
    deserialize.c deserialize.h
    test_helpers.c test_helpers.h
    xref.c xref.h)
target_include_directories(pdf PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(pdf PUBLIC arena pdf-error)
target_link_libraries(pdf PRIVATE pdf-logger pdf-test)
target_compile_features(pdf PUBLIC c_std_23)
target_compile_options(pdf PRIVATE -fsanitize=address)
target_link_options(pdf PRIVATE -fsanitize=address)
