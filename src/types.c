#include "pdf/types.h"

#include "log.h"
#include "pdf/error.h"
#include "pdf/object.h"

PdfError* pdf_deserialize_number(PdfObject* object, PdfNumber* deserialized) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(deserialized);

    switch (object->type) {
        case PDF_OBJECT_TYPE_INTEGER: {
            deserialized->type = PDF_NUMBER_TYPE_INTEGER;
            deserialized->value.integer = object->data.integer;
            break;
        }
        case PDF_OBJECT_TYPE_REAL: {
            deserialized->type = PDF_NUMBER_TYPE_REAL;
            deserialized->value.real = object->data.real;
            break;
        }
        default: {
            return PDF_ERROR(
                PDF_ERR_INCORRECT_TYPE,
                "Numbers must be either integers or reals"
            );
        }
    }

    return NULL;
}

PdfError* pdf_deserialize_number_wrapper(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
) {
    (void)arena;
    (void)resolver;

    return pdf_deserialize_number(object, deserialized);
}
