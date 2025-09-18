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
    PdfFormXObject* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(arena);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Type",
            &target_ptr->type,
            PDF_DESERDE_OPTIONAL(
                pdf_name_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
            )
        ),
        PDF_FIELD(
            "Subtype",
            &target_ptr->subtype,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "FormType",
            &target_ptr->form_type,
            PDF_DESERDE_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "BBox",
            &target_ptr->bbox,
            PDF_DESERDE_CUSTOM(pdf_deserialize_rectangle_trampoline)
        ),
        PDF_UNIMPLEMENTED_FIELD("Matrix"),
        PDF_FIELD(
            "Resources",
            &target_ptr->resources,
            PDF_DESERDE_OPTIONAL(
                pdf_resources_op_init,
                PDF_DESERDE_CUSTOM(pdf_deserialize_resources_trampoline)
            )
        ),
        PDF_FIELD(
            "Group",
            &target_ptr->group,
            PDF_DESERDE_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_DICT)
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

    PDF_PROPAGATE(pdf_deserialize_dict(
        resolved.data.stream.stream_dict->raw_dict,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        true,
        resolver,
        arena,
        "PdfFormXObject"
    ));

    PDF_PROPAGATE(pdf_deserialize_content_stream(
        &resolved,
        &target_ptr->content_stream,
        resolver,
        arena
    ));

    if (target_ptr->type.has_value
        && strcmp(target_ptr->type.value, "XObject") != 0) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect type `%s`",
            target_ptr->subtype
        );
    }

    if (strcmp(target_ptr->subtype, "Form") != 0) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect subtype `%s`",
            target_ptr->subtype
        );
    }

    return NULL;
}

typedef struct {
    PdfName subtype;
} XObjectUntyped;

PdfError* pdf_deserialize_xobject(
    const PdfObject* object,
    PdfXObject* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(arena);

    XObjectUntyped untyped = {0};
    PdfFieldDescriptor fields[] = {PDF_FIELD(
        "Subtype",
        &untyped.subtype,
        PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
    )};

    PdfObject resolved;
    PDF_PROPAGATE(pdf_resolve_object(resolver, object, &resolved));
    if (resolved.type != PDF_OBJECT_TYPE_STREAM) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Expected xobject to be a stream"
        );
    }

    target_ptr->raw_object = object;
    PDF_PROPAGATE(pdf_deserialize_dict(
        resolved.data.stream.stream_dict->raw_dict,
        fields,
        true,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        resolver,
        arena,
        "XObjectUntyped"
    ));

    if (strcmp(untyped.subtype, "Form") == 0) {
        target_ptr->type = PDF_XOBJECT_FORM;
        PDF_PROPAGATE(pdf_deserialize_form_xobject(
            object,
            &target_ptr->data.form,
            resolver,
            arena
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
