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

PdfError* pdf_deserialize_gstate_params(
    const PdfObject* object,
    PdfGStateParams* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(arena);

    PdfFieldDescriptor fields[] = {
        PDF_UNIMPLEMENTED_FIELD("Type"),
        PDF_UNIMPLEMENTED_FIELD("LW"),
        PDF_UNIMPLEMENTED_FIELD("LC"),
        PDF_UNIMPLEMENTED_FIELD("LJ"),
        PDF_UNIMPLEMENTED_FIELD("ML"),
        PDF_UNIMPLEMENTED_FIELD("D"),
        PDF_UNIMPLEMENTED_FIELD("RI"),
        PDF_UNIMPLEMENTED_FIELD("OP"),
        PDF_UNIMPLEMENTED_FIELD("op"),
        PDF_UNIMPLEMENTED_FIELD("OPM"),
        PDF_UNIMPLEMENTED_FIELD("Font"),
        PDF_UNIMPLEMENTED_FIELD("BG"),
        PDF_UNIMPLEMENTED_FIELD("BG2"),
        PDF_UNIMPLEMENTED_FIELD("UCR"),
        PDF_UNIMPLEMENTED_FIELD("UCR2"),
        PDF_UNIMPLEMENTED_FIELD("TR"),
        PDF_UNIMPLEMENTED_FIELD("TR2"),
        PDF_UNIMPLEMENTED_FIELD("HT"),
        PDF_UNIMPLEMENTED_FIELD("FL"),
        PDF_UNIMPLEMENTED_FIELD("SM"),
        PDF_UNIMPLEMENTED_FIELD("SA"),
        PDF_UNIMPLEMENTED_FIELD("BM"),
        PDF_UNIMPLEMENTED_FIELD("SMask"),
        PDF_FIELD(
            "CA",
            &target_ptr->ca_stroking,
            PDF_DESERDE_OPTIONAL(
                pdf_real_op_init,
                PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
            )
        ),
        PDF_FIELD(
            "ca",
            &target_ptr->ca_nonstroking,
            PDF_DESERDE_OPTIONAL(
                pdf_real_op_init,
                PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
            )
        ),
        PDF_UNIMPLEMENTED_FIELD("AIS"),
        PDF_UNIMPLEMENTED_FIELD("TK")
    };

    PDF_PROPAGATE(pdf_deserialize_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        arena,
        "PdfGStateParams"
    ));

    return NULL;
}
