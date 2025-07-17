#include <stddef.h>

#include "deserialize.h"
#include "pdf_catalog.h"
#include "pdf_doc.h"
#include "pdf_object.h"
#include "pdf_trailer.h"

PdfTrailer* pdf_deserialize_trailer(
    PdfObject* object,
    PdfDocument* doc,
    PdfResult* result
) {
    if (!object || !result) {
        return NULL;
    }

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfTrailer,
            "Size",
            size,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(PdfTrailer, "Root", root, PDF_REF_FIELD(PdfCatalogRef))
    };

    PdfTrailer trailer = {.raw_dict = object};
    *result = pdf_deserialize_object(
        &trailer,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        doc
    );

    if (*result != PDF_OK) {
        return NULL;
    }

    PdfTrailer* allocated = arena_alloc(pdf_doc_arena(doc), sizeof(PdfTrailer));
    *allocated = trailer;

    return allocated;
}
