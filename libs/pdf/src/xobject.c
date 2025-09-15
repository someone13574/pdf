#include "pdf/xobject.h"

#include <string.h>

#include "deserialize.h"
#include "logger/log.h"
#include "pdf/content_stream.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_form_xobject(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfFormXObject* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfFormXObject,
            "Type",
            type,
            PDF_OPTIONAL_FIELD(
                PdfOpName,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
            )
        ),
        PDF_FIELD(
            PdfFormXObject,
            "Subtype",
            subtype,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            PdfFormXObject,
            "FormType",
            form_type,
            PDF_OPTIONAL_FIELD(
                PdfOpInteger,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            PdfFormXObject,
            "BBox",
            bbox,
            PDF_CUSTOM_FIELD(pdf_deserialize_rectangle_wrapper)
        ),
        PDF_UNIMPLEMENTED_FIELD("Matrix"),
        PDF_FIELD(
            PdfFormXObject,
            "Resources",
            resources,
            PDF_OPTIONAL_FIELD(
                PdfOpResources,
                PDF_CUSTOM_FIELD(pdf_deserialize_resources_wrapper)
            )
        ),
        PDF_FIELD(
            PdfFormXObject,
            "Group",
            group,
            PDF_OPTIONAL_FIELD(
                PdfOpDict,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_UNIMPLEMENTED_FIELD("Ref"),
        PDF_UNIMPLEMENTED_FIELD("Metadata"),
        PDF_UNIMPLEMENTED_FIELD("PieceInfo"),
        PDF_UNIMPLEMENTED_FIELD("LastModified"),
        PDF_UNIMPLEMENTED_FIELD("StructParent"),
        PDF_UNIMPLEMENTED_FIELD("StructParents"),
        PDF_UNIMPLEMENTED_FIELD("OPI"),
        PDF_UNIMPLEMENTED_FIELD("OC"),
        PDF_UNIMPLEMENTED_FIELD("Name")
    };

    PdfObject resolved;
    PDF_PROPAGATE(pdf_resolve_object(resolver, object, &resolved));
    if (resolved.type != PDF_OBJECT_TYPE_STREAM) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Expected xobject to be a stream"
        );
    }

    PDF_PROPAGATE(pdf_deserialize_object(
        deserialized,
        resolved.data.stream.stream_dict->raw_dict,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        resolver,
        true,
        "PdfFormXObject"
    ));

    PDF_PROPAGATE(pdf_deserialize_content_stream(
        &resolved.data.stream,
        arena,
        &deserialized->content_stream
    ));

    if (deserialized->type.discriminant
        && strcmp(deserialized->type.value, "XObject") != 0) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect type `%s`",
            deserialized->subtype
        );
    }

    if (strcmp(deserialized->subtype, "Form") != 0) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect subtype `%s`",
            deserialized->subtype
        );
    }

    return NULL;
}

typedef struct {
    PdfName subtype;
} XObjectUntyped;

PdfError* pdf_deserialize_xobject(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfXObject* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {PDF_FIELD(
        XObjectUntyped,
        "Subtype",
        subtype,
        PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
    )};

    PdfObject resolved;
    PDF_PROPAGATE(pdf_resolve_object(resolver, object, &resolved));
    if (resolved.type != PDF_OBJECT_TYPE_STREAM) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Expected xobject to be a stream"
        );
    }

    deserialized->raw_object = object;

    XObjectUntyped untyped = {0};
    PDF_PROPAGATE(pdf_deserialize_object(
        &untyped,
        resolved.data.stream.stream_dict->raw_dict,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        resolver,
        true,
        "XObjectUntyped"
    ));

    if (strcmp(untyped.subtype, "Form") == 0) {
        deserialized->type = PDF_XOBJECT_FORM;
        PDF_PROPAGATE(pdf_deserialize_form_xobject(
            object,
            arena,
            resolver,
            &deserialized->data.form
        ));
    } else if (strcmp(untyped.subtype, "Image") == 0) {
        LOG_TODO();
    } else {
        return PDF_ERROR(
            PDF_ERR_INVALID_SUBTYPE,
            "Invalid xobject subtype `%s`",
            untyped.subtype
        );
    }

    return NULL;
}
