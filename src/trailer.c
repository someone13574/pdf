#include "pdf/trailer.h"

#include "arena/arena.h"
#include "deserialize.h"
#include "log.h"
#include "pdf/catalog.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/result.h"

PdfResult pdf_deserialize_trailer(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfTrailer* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfTrailer,
            "Size",
            size,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(PdfTrailer, "Root", root, PDF_REF_FIELD(PdfCatalogRef))
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
