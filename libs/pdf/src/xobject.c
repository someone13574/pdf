#include "pdf/xobject.h"

#include <string.h>

#include "deser.h"
#include "err/error.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources.h"

Error* pdf_deser_form_xobject(
    const PdfObject* object,
    PdfFormXObject* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Type",
            &target_ptr->type,
            PDF_DESER_OPTIONAL(
                pdf_name_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
            )
        ),
        PDF_FIELD(
            "Subtype",
            &target_ptr->subtype,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "FormType",
            &target_ptr->form_type,
            PDF_DESER_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "BBox",
            &target_ptr->bbox,
            PDF_DESER_CUSTOM(pdf_deser_rectangle_trampoline)
        ),
        PDF_FIELD(
            "Matrix",
            &target_ptr->matrix,
            PDF_DESER_OPTIONAL(
                pdf_geom_mat3_op_init,
                PDF_DESER_CUSTOM(pdf_deser_pdf_mat_trampoline)
            )
        ),
        PDF_FIELD(
            "Resources",
            &target_ptr->resources,
            PDF_DESER_OPTIONAL(
                pdf_resources_op_init,
                PDF_DESER_CUSTOM(pdf_deser_resources_trampoline)
            )
        ),
        PDF_FIELD(
            "Group",
            &target_ptr->group,
            PDF_DESER_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_DICT)
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
    TRY(pdf_resolve_object(resolver, object, &resolved, true));
    if (resolved.type != PDF_OBJECT_TYPE_STREAM) {
        return ERROR(PDF_ERR_INCORRECT_TYPE, "Expected xobject to be a stream");
    }

    TRY(pdf_deser_dict(
        resolved.data.stream.stream_dict->raw_dict,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        true,
        resolver,
        "PdfFormXObject"
    ));

    TRY(pdf_deser_content_stream(
            &resolved,
            &target_ptr->content_stream,
            resolver
        ),
        "Failed to deser form context stream");

    if (target_ptr->type.has_value
        && strcmp(target_ptr->type.value, "XObject") != 0) {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect type `%s`",
            target_ptr->subtype
        );
    }

    if (strcmp(target_ptr->subtype, "Form") != 0) {
        return ERROR(
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

Error* pdf_deser_xobject(
    const PdfObject* object,
    PdfXObject* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    XObjectUntyped untyped = {0};
    PdfFieldDescriptor fields[] = {PDF_FIELD(
        "Subtype",
        &untyped.subtype,
        PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
    )};

    PdfObject resolved;
    TRY(pdf_resolve_object(resolver, object, &resolved, true));
    if (resolved.type != PDF_OBJECT_TYPE_STREAM) {
        return ERROR(PDF_ERR_INCORRECT_TYPE, "Expected xobject to be a stream");
    }

    TRY(pdf_deser_dict(
        resolved.data.stream.stream_dict->raw_dict,
        fields,
        true,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        resolver,
        "XObjectUntyped"
    ));

    if (strcmp(untyped.subtype, "Form") == 0) {
        target_ptr->type = PDF_XOBJECT_FORM;
        TRY(pdf_deser_form_xobject(object, &target_ptr->data.form, resolver));
    } else if (strcmp(untyped.subtype, "Image") == 0) {
        LOG_TODO();
    } else {
        return ERROR(
            PDF_ERR_INVALID_SUBTYPE,
            "Invalid xobject subtype `%s`",
            untyped.subtype
        );
    }

    return NULL;
}
