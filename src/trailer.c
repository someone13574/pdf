#include <stddef.h>

#include "deserialize.h"
#include "pdf_catalog.h"
#include "pdf_object.h"
#include "pdf_trailer.h"

PdfTrailer*
pdf_deserialize_trailer(PdfObject* object, Arena* arena, PdfResult* result) {
    if (!object || !result) {
        return NULL;
    }

    PdfFieldDescriptor fields[] = {
        PDF_OBJECT_FIELD(PdfTrailer, "Size", size, PDF_OBJECT_TYPE_INTEGER),
        PDF_REF_FIELD(PdfTrailer, "Root", root, PdfCatalogRef)
    };

    PdfTrailer trailer = {.raw_dict = object};
    *result = pdf_deserialize_object(
        &trailer,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        object
    );

    if (*result != PDF_OK) {
        return NULL;
    }

    PdfTrailer* allocated = arena_alloc(arena, sizeof(PdfTrailer));
    *allocated = trailer;

    return allocated;
}
