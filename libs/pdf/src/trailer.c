#include "pdf/trailer.h"

#include "arena/arena.h"
#include "deser.h"
#include "err/error.h"
#include "logger/log.h"
#include "pdf/catalog.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

Error* pdf_deser_trailer(
    const PdfObject* object,
    PdfTrailer* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(target_ptr);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Size",
            &target_ptr->size,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(
            "Prev",
            &target_ptr->prev,
            PDF_DESER_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "Root",
            &target_ptr->root,
            PDF_DESER_RESOLVABLE(pdf_catalog_ref_init)
        ),
        PDF_FIELD(
            "Info",
            &target_ptr->info,
            PDF_DESER_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "ID",
            &target_ptr->id,
            PDF_DESER_OPTIONAL(
                pdf_array_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_ARRAY)
            )
        )
    };

    TRY(pdf_deser_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfTrailer"
    ));

    return NULL;
}
