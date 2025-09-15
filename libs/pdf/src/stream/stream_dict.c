#include "pdf/stream/stream_dict.h"

#include "../deserialize.h"
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
        )
    };

    deserialized->raw_dict = object;
    PDF_PROPAGATE(pdf_deserialize_object(
        deserialized,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        pdf_op_resolver_none(false),
        false,
        "PdfStreamDict"
    ));

    return NULL;
}
