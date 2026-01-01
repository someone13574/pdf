#include "pdf/fonts/stream_dict.h"

#include <stdbool.h>

#include "../deser.h"
#include "err/error.h"
#include "logger/log.h"

Error* pdf_deserde_font_stream_dict(
    const PdfObject* object,
    PdfFontStreamDict* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Length1",
            &target_ptr->length1,
            PDF_DESERDE_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "Length2",
            &target_ptr->length2,
            PDF_DESERDE_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "Length3",
            &target_ptr->length3,
            PDF_DESERDE_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "Subtype",
            &target_ptr->subtype,
            PDF_DESERDE_OPTIONAL(
                pdf_name_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
            )
        ),
        PDF_FIELD(
            "Metadata",
            &target_ptr->metadata,
            PDF_DESERDE_OPTIONAL(
                pdf_stream_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_STREAM)
            )
        )
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        true,
        resolver,
        "FontStreamDict"
    ));

    return NULL;
}
