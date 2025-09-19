#include "pdf/resources.h"

#include "deserialize.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

PdfError* pdf_deserialize_resources(
    const PdfObject* object,
    PdfResources* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(arena);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "ExtGState",
            &target_ptr->ext_gstate,
            PDF_DESERDE_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "ColorSpace",
            &target_ptr->color_space,
            PDF_DESERDE_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "Pattern",
            &target_ptr->pattern,
            PDF_DESERDE_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "Shading",
            &target_ptr->shading,
            PDF_DESERDE_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "XObject",
            &target_ptr->xobject,
            PDF_DESERDE_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "Font",
            &target_ptr->font,
            PDF_DESERDE_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "ProcSet",
            &target_ptr->proc_set,
            PDF_DESERDE_OPTIONAL(
                pdf_name_vec_op_init,
                PDF_DESERDE_ARRAY(
                    pdf_name_vec_push_uninit,
                    PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
                )
            )
        )
    };

    PDF_PROPAGATE(pdf_deserialize_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        arena,
        "PdfResources"
    ));

    return NULL;
}

DESERDE_IMPL_TRAMPOLINE(
    pdf_deserialize_resources_trampoline,
    pdf_deserialize_resources
)
DESERDE_IMPL_OPTIONAL(PdfResourcesOptional, pdf_resources_op_init)
