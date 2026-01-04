#include "pdf/fonts/stream_dict.h"

#include <stdbool.h>

#include "err/error.h"
#include "logger/log.h"
#include "pdf/types.h"

Error* pdf_deserde_font_stream_dict(
    const PdfObject* object,
    PdfFontStreamDict* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_integer_optional_field("Length1", &target_ptr->length1),
        pdf_integer_optional_field("Length2", &target_ptr->length2),
        pdf_integer_optional_field("Length3", &target_ptr->length3),
        pdf_name_optional_field("Subtype", &target_ptr->subtype),
        pdf_stream_optional_field("Metadata", &target_ptr->metadata)
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
