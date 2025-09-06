#include "pdf/catalog.h"

#include "deserialize.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/page.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_catalog(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfCatalog* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfCatalog,
            "Type",
            type,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(PdfCatalog, "Pages", pages, PDF_REF_FIELD(PdfPageTreeNodeRef))
    };

    deserialized->raw_dict = object;
    PDF_PROPAGATE(pdf_deserialize_object(
        deserialized,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        resolver
    ));

    return NULL;
}

PDF_DESERIALIZABLE_REF_IMPL(PdfCatalog, catalog, pdf_deserialize_catalog)
