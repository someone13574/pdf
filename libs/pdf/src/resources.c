#include "pdf/resources.h"

#include "deserialize.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

PdfError* pdf_deserialize_resources(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfResources* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfResources,
            "ExtGState",
            ext_gstate,
            PDF_OPTIONAL_FIELD(
                PdfOpDict,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            PdfResources,
            "ColorSpace",
            color_space,
            PDF_OPTIONAL_FIELD(
                PdfOpDict,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            PdfResources,
            "Pattern",
            pattern,
            PDF_OPTIONAL_FIELD(
                PdfOpDict,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            PdfResources,
            "Shading",
            shading,
            PDF_OPTIONAL_FIELD(
                PdfOpDict,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            PdfResources,
            "XObject",
            xobject,
            PDF_OPTIONAL_FIELD(
                PdfOpDict,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            PdfResources,
            "Font",
            font,
            PDF_OPTIONAL_FIELD(
                PdfOpDict,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            PdfResources,
            "ProcSet",
            proc_set,
            PDF_OPTIONAL_FIELD(
                PdfOpNameArray,
                PDF_ARRAY_FIELD(
                    PdfNameArray,
                    PdfName,
                    PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
                )
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
        resolver,
        false,
        "PdfResources"
    ));

    return NULL;
}

PDF_UNTYPED_DESERIALIZER_WRAPPER(
    pdf_deserialize_resources,
    pdf_deserialize_resources_wrapper
)
