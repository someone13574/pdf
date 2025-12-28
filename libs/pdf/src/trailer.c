#include "pdf/trailer.h"

#include "arena/arena.h"
#include "deserialize.h"
#include "logger/log.h"
#include "pdf/catalog.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_trailer(
    const PdfObject* object,
    PdfTrailer* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(target_ptr);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Size",
            &target_ptr->size,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(
            "Prev",
            &target_ptr->prev,
            PDF_DESERDE_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "Root",
            &target_ptr->root,
            PDF_DESERDE_RESOLVABLE(pdf_catalog_ref_init)
        ),
        PDF_FIELD(
            "Info",
            &target_ptr->info,
            PDF_DESERDE_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "ID",
            &target_ptr->id,
            PDF_DESERDE_OPTIONAL(
                pdf_array_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_ARRAY)
            )
        )
    };

    PDF_PROPAGATE(pdf_deserialize_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        arena,
        "PdfTrailer"
    ));

    return NULL;
}
