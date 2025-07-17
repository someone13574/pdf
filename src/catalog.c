#include <stddef.h>

#include "deserialize.h"
#include "pdf_catalog.h"
#include "pdf_doc.h"
#include "pdf_object.h"
#include "pdf_page.h"
#include "pdf_result.h"

PdfCatalog* pdf_deserialize_catalog(
    PdfObject* object,
    PdfDocument* doc,
    PdfResult* result
) {
    if (!object || !doc || !result) {
        return NULL;
    }

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfCatalog,
            "Type",
            type,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(PdfCatalog, "Pages", pages, PDF_REF_FIELD(PdfPageTreeNodeRef))
    };

    PdfCatalog catalog = {.raw_dict = object};
    *result = pdf_deserialize_object(
        &catalog,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        doc
    );

    if (*result != PDF_OK) {
        return NULL;
    }

    PdfCatalog* allocated = arena_alloc(pdf_doc_arena(doc), sizeof(PdfCatalog));
    *allocated = catalog;

    return allocated;
}

PDF_DESERIALIZABLE_REF_IMPL(PdfCatalog, catalog, pdf_deserialize_catalog)
