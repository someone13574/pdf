#include "pdf/resources.h"

#include <string.h>

#include "deser.h"
#include "err/error.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

Error* pdf_deser_resources(
    const PdfObject* object,
    PdfResources* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "ExtGState",
            &target_ptr->ext_gstate,
            PDF_DESER_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "ColorSpace",
            &target_ptr->color_space,
            PDF_DESER_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "Pattern",
            &target_ptr->pattern,
            PDF_DESER_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "Shading",
            &target_ptr->shading,
            PDF_DESER_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "XObject",
            &target_ptr->xobject,
            PDF_DESER_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "Font",
            &target_ptr->font,
            PDF_DESER_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "ProcSet",
            &target_ptr->proc_set,
            PDF_DESER_OPTIONAL(
                pdf_name_vec_op_init,
                PDF_DESER_ARRAY(
                    pdf_name_vec_push_uninit,
                    PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
                )
            )
        ),
        PDF_FIELD(
            "Properties",
            &target_ptr->properties,
            PDF_DESER_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
    };

    TRY(pdf_deser_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfResources"
    ));

    return NULL;
}

DESER_IMPL_TRAMPOLINE(pdf_deser_resources_trampoline, pdf_deser_resources)
DESER_IMPL_OPTIONAL(PdfResourcesOptional, pdf_resources_op_init)

Error* pdf_deser_gstate_params(
    const PdfObject* object,
    PdfGStateParams* target_ptr,
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
        PDF_UNIMPLEMENTED_FIELD("LW"),
        PDF_UNIMPLEMENTED_FIELD("LC"),
        PDF_UNIMPLEMENTED_FIELD("LJ"),
        PDF_UNIMPLEMENTED_FIELD("ML"),
        PDF_UNIMPLEMENTED_FIELD("D"),
        PDF_UNIMPLEMENTED_FIELD("RI"),
        PDF_FIELD(
            "OP",
            &target_ptr->overprint_upper,
            PDF_DESER_OPTIONAL(
                pdf_boolean_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_BOOLEAN)
            )
        ),
        PDF_FIELD(
            "op",
            &target_ptr->overprint_lower,
            PDF_DESER_OPTIONAL(
                pdf_boolean_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_BOOLEAN)
            )
        ),
        PDF_FIELD(
            "OPM",
            &target_ptr->overprint_mode,
            PDF_DESER_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_UNIMPLEMENTED_FIELD("Font"),
        PDF_UNIMPLEMENTED_FIELD("BG"),
        PDF_UNIMPLEMENTED_FIELD("BG2"),
        PDF_UNIMPLEMENTED_FIELD("UCR"),
        PDF_UNIMPLEMENTED_FIELD("UCR2"),
        PDF_IGNORED_FIELD("TR", &target_ptr->tr), // TODO: functions
        PDF_UNIMPLEMENTED_FIELD("TR2"),
        PDF_UNIMPLEMENTED_FIELD("HT"),
        PDF_UNIMPLEMENTED_FIELD("FL"),
        PDF_FIELD(
            "SM",
            &target_ptr->sm,
            PDF_DESER_OPTIONAL(
                pdf_real_op_init,
                PDF_DESER_CUSTOM(pdf_deser_num_as_real_trampoline)
            )
        ),
        PDF_FIELD(
            "SA",
            &target_ptr->sa,
            PDF_DESER_OPTIONAL(
                pdf_boolean_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_BOOLEAN)
            )
        ),
        PDF_IGNORED_FIELD("BM", &target_ptr->bm),       // TODO: blend mode
        PDF_IGNORED_FIELD("SMask", &target_ptr->smask), // TODO: masking
        PDF_FIELD(
            "CA",
            &target_ptr->ca_stroking,
            PDF_DESER_OPTIONAL(
                pdf_real_op_init,
                PDF_DESER_CUSTOM(pdf_deser_num_as_real_trampoline)
            )
        ),
        PDF_FIELD(
            "ca",
            &target_ptr->ca_nonstroking,
            PDF_DESER_OPTIONAL(
                pdf_real_op_init,
                PDF_DESER_CUSTOM(pdf_deser_num_as_real_trampoline)
            )
        ),
        PDF_FIELD(
            "AIS",
            &target_ptr->ais,
            PDF_DESER_OPTIONAL(
                pdf_boolean_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_BOOLEAN)
            )
        ),
        PDF_UNIMPLEMENTED_FIELD("TK")
    };

    TRY(pdf_deser_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfGStateParams"
    ));

    if (target_ptr->type.has_value) {
        if (strcmp(target_ptr->type.value, "ExtGState") != 0) {
            return ERROR(PDF_ERR_INVALID_SUBTYPE, "`Type` must be `ExtGState`");
        }
    }

    return NULL;
}
