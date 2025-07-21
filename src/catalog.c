#include "deserialize.h"
#include "log.h"
#include "pdf_catalog.h"
#include "pdf_object.h"
#include "pdf_page.h"
#include "pdf_resolver.h"
#include "pdf_result.h"

PdfResult pdf_deserialize_catalog(
    PdfObject* object,
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

    return PDF_OK;
}

PDF_DESERIALIZABLE_REF_IMPL(PdfCatalog, catalog, pdf_deserialize_catalog)
