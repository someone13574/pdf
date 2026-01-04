#include "pdf/xobject.h"

#include <string.h>

#include "err/error.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources.h"
#include "pdf/stream_dict.h"
#include "pdf/types.h"

Error* pdf_deserde_form_xobject(
    const PdfObject* object,
    PdfFormXObject* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_name_optional_field("Type", &target_ptr->type),
        pdf_name_field("Subtype", &target_ptr->subtype),
        pdf_integer_optional_field("FormType", &target_ptr->form_type),
        pdf_rectangle_field("BBox", &target_ptr->bbox),
        pdf_pdf_mat_optional_field("Matrix", &target_ptr->matrix),
        pdf_resources_optional_field("Resources", &target_ptr->resources),
        pdf_dict_optional_field("Group", &target_ptr->group),
        pdf_unimplemented_field("Ref"),
        pdf_unimplemented_field("Metadata"),
        pdf_unimplemented_field("PieceInfo"),
        pdf_unimplemented_field("LastModified"),
        pdf_unimplemented_field("StructParent"),
        pdf_unimplemented_field("StructParents"),
        pdf_unimplemented_field("OPI"),
        pdf_unimplemented_field("OC"),
        pdf_unimplemented_field("Name")
    };

    PdfObject resolved;
    TRY(pdf_resolve_object(resolver, object, &resolved, true));
    if (resolved.type != PDF_OBJECT_TYPE_STREAM) {
        return ERROR(PDF_ERR_INCORRECT_TYPE, "Expected xobject to be a stream");
    }

    TRY(pdf_deserde_fields(
        resolved.data.stream.stream_dict->raw_dict,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        true,
        resolver,
        "PdfFormXObject"
    ));

    TRY(pdf_deserde_content_stream(
            &resolved,
            &target_ptr->content_stream,
            resolver
        ),
        "Failed to deserialize form context stream");

    if (target_ptr->type.is_some
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

Error* pdf_deserde_xobject(
    const PdfObject* object,
    PdfXObject* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfName subtype;
    PdfFieldDescriptor fields[] = {
        pdf_name_field("Subtype", &subtype),
    };

    PdfObject resolved;
    TRY(pdf_resolve_object(resolver, object, &resolved, true));
    if (resolved.type != PDF_OBJECT_TYPE_STREAM) {
        return ERROR(PDF_ERR_INCORRECT_TYPE, "Expected xobject to be a stream");
    }

    TRY(pdf_deserde_fields(
        resolved.data.stream.stream_dict->raw_dict,
        fields,
        true,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        resolver,
        "XObjectUntyped"
    ));

    if (strcmp(subtype, "Form") == 0) {
        target_ptr->type = PDF_XOBJECT_FORM;
        TRY(pdf_deserde_form_xobject(object, &target_ptr->data.form, resolver));
    } else if (strcmp(subtype, "Image") == 0) {
        LOG_TODO();
    } else {
        return ERROR(
            PDF_ERR_INVALID_SUBTYPE,
            "Invalid xobject subtype `%s`",
            subtype
        );
    }

    return NULL;
}
