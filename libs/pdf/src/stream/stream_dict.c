#include <string.h>

#include "../deser.h"
#include "arena/arena.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

Error* pdf_deserde_stream_dict(
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
        // Panic if we find any of these fields
        pdf_unimplemented_field("DecodeParams"),
        pdf_unimplemented_field("F"),
        pdf_unimplemented_field("FFilter"),
        pdf_unimplemented_field("FDecodeParams"),
        pdf_unimplemented_field("DL")
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
        DESER,
        "Stream dict:\n%s\n",
        pdf_fmt_object(temp_arena, copied_object)
    );
    arena_free(temp_arena);

    TRY(pdf_deserde_fields(
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
