#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../deserialize.h"
#include "arena/arena.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

PdfError* pdf_deserialize_stream_dict(
    const PdfObject* object,
    Arena* arena,
    PdfStreamDict* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfStreamDict,
            "Length",
            length,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(
            PdfStreamDict,
            "Filter",
            filter,
            PDF_OPTIONAL_FIELD(
                PdfOpNameArray,
                PDF_AS_ARRAY_FIELD(
                    PdfNameArray,
                    PdfName,
                    PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
                )
            )
        ),
        PDF_FIELD(
            PdfStreamDict,
            "Length1",
            length1,
            PDF_OPTIONAL_FIELD(
                PdfOpInteger,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            PdfStreamDict,
            "Length2",
            length2,
            PDF_OPTIONAL_FIELD(
                PdfOpInteger,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            PdfStreamDict,
            "Length3",
            length3,
            PDF_OPTIONAL_FIELD(
                PdfOpInteger,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            PdfStreamDict,
            "Subtype",
            subtype,
            PDF_OPTIONAL_FIELD(
                PdfOpName,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
            )
        ),
        PDF_FIELD(
            PdfStreamDict,
            "Metadata",
            metadata,
            PDF_OPTIONAL_FIELD(
                PdfOpStream,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STREAM)
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
    PdfObject* copied_object = arena_alloc(arena, sizeof(PdfObject));
    memcpy(copied_object, object, sizeof(PdfObject));
    deserialized->raw_dict = copied_object;

    LOG_DIAG(
        INFO,
        DESERDE,
        "Stream dict:\n%s\n",
        pdf_fmt_object(arena, copied_object)
    );

    PDF_PROPAGATE(pdf_deserialize_object(
        deserialized,
        copied_object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        pdf_op_resolver_none(false),
        true,
        "PdfStreamDict"
    ));

    RELEASE_ASSERT(deserialized->length != 0);

    return NULL;
}
