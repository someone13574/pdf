#include "pdf/fonts/stream_dict.h"

#include <stdbool.h>

#include "../deserialize.h"
#include "logger/log.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_font_stream_dict(
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

    PDF_PROPAGATE(pdf_deserialize_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        true,
        resolver,
        "FontStreamDict"
    ));

    return NULL;
}
