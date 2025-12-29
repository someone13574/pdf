#include <string.h>

#include "../deserialize.h"
#include "arena/arena.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

PdfError* pdf_deserialize_stream_dict(
    const PdfObject* object,
    PdfStreamDict* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Length",
            &target_ptr->length,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(
            "Filter",
            &target_ptr->filter,
            PDF_DESERDE_OPTIONAL(
                pdf_name_vec_op_init,
                PDF_DESERDE_AS_ARRAY(
                    pdf_name_vec_push_uninit,
                    PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
                )
            )
        ),
        PDF_FIELD(
            "Length1",
            &target_ptr->length1,
            PDF_DESERDE_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "Length2",
            &target_ptr->length2,
            PDF_DESERDE_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "Length3",
            &target_ptr->length3,
            PDF_DESERDE_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "Subtype",
            &target_ptr->subtype,
            PDF_DESERDE_OPTIONAL(
                pdf_name_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
            )
        ),
        PDF_FIELD(
            "Metadata",
            &target_ptr->metadata,
            PDF_DESERDE_OPTIONAL(
                pdf_stream_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_STREAM)
            )
        ),
        // Panic if we find any of these fields
        PDF_UNIMPLEMENTED_FIELD("DecodeParams"),
        PDF_UNIMPLEMENTED_FIELD("F"),
        PDF_UNIMPLEMENTED_FIELD("FFilter"),
        PDF_UNIMPLEMENTED_FIELD("FDecodeParams"),
        PDF_UNIMPLEMENTED_FIELD("DL")
    };

    // Need to do a copy since the stream will take over the original
    // object.
    PdfObject* copied_object =
        arena_alloc(pdf_resolver_arena(resolver), sizeof(PdfObject));
    memcpy(copied_object, object, sizeof(PdfObject));
    target_ptr->raw_dict = copied_object;

    Arena* temp_arena = arena_new(128);
    LOG_DIAG(
        INFO,
        DESERDE,
        "Stream dict:\n%s\n",
        pdf_fmt_object(temp_arena, copied_object)
    );
    arena_free(temp_arena);

    PDF_PROPAGATE(pdf_deserialize_dict(
        copied_object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        true,
        resolver,
        "PdfStreamDict"
    ));

    RELEASE_ASSERT(target_ptr->length != 0);

    return NULL;
}
