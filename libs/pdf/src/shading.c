#include "pdf/shading.h"

#include <stdio.h>

#include "deser.h"
#include "err/error.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

Error* pdf_deserde_shading_dict(
    const PdfObject* object,
    PdfShadingDict* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "ShadingType",
            &target_ptr->shading_type,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(
            "ColorSpace",
            &target_ptr->color_space,
            PDF_DESERDE_CUSTOM(pdf_deserde_color_space_trampoline)
        ),
        pdf_unimplemented_field("Background"),
        PDF_FIELD(
            "BBox",
            &target_ptr->bbox,
            PDF_DESERDE_OPTIONAL(
                pdf_rectangle_op_init,
                PDF_DESERDE_CUSTOM(pdf_deserde_rectangle_trampoline)
            )
        ),
        PDF_FIELD(
            "AntiAlias",
            &target_ptr->anti_alias,
            PDF_DESERDE_OPTIONAL(
                pdf_boolean_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_BOOLEAN)
            )
        )
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        true,
        resolver,
        "PdfShaderDict"
    ));

    switch (target_ptr->shading_type) {
        case 2: {
            PdfFieldDescriptor specific_fields[] = {
                pdf_ignored_field("ShadingType", NULL),
                pdf_ignored_field("ColorSpace", NULL),
                pdf_ignored_field("Background", NULL),
                pdf_ignored_field("BBox", NULL),
                pdf_ignored_field("AntiAlias", NULL),
                PDF_FIELD(
                    "Coords",
                    &target_ptr->data.type2.coords,
                    PDF_DESERDE_ARRAY(
                        pdf_number_vec_push_uninit,
                        PDF_DESERDE_CUSTOM(pdf_deserde_number_trampoline)
                    )
                ),
                PDF_FIELD(
                    "Domain",
                    &target_ptr->data.type2.domain,
                    PDF_DESERDE_OPTIONAL(
                        pdf_number_vec_op_init,
                        PDF_DESERDE_ARRAY(
                            pdf_number_vec_push_uninit,
                            PDF_DESERDE_CUSTOM(pdf_deserde_number_trampoline)
                        )
                    )
                ),
                PDF_FIELD(
                    "Function",
                    &target_ptr->data.type2.function,
                    PDF_DESERDE_AS_ARRAY(
                        pdf_function_vec_push_uninit,
                        PDF_DESERDE_CUSTOM(pdf_deserde_function_trampoline)
                    )
                ),
                PDF_FIELD(
                    "Extend",
                    &target_ptr->data.type2.extend,
                    PDF_DESERDE_OPTIONAL(
                        pdf_boolean_vec_op_init,
                        PDF_DESERDE_ARRAY(
                            pdf_boolean_vec_push_uninit,
                            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_BOOLEAN)
                        )
                    )
                )
            };

            TRY(pdf_deserde_fields(
                object,
                specific_fields,
                sizeof(specific_fields) / sizeof(PdfFieldDescriptor),
                false,
                resolver,
                "Type2 shading dict"
            ));

            if (pdf_number_vec_len(target_ptr->data.type2.coords) != 4) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            if (target_ptr->data.type2.domain.has_value
                && pdf_number_vec_len(target_ptr->data.type2.domain.value)
                       != 2) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            if (target_ptr->data.type2.extend.has_value
                && pdf_boolean_vec_len(target_ptr->data.type2.extend.value)
                       != 2) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            break;
        }
        case 3: {
            PdfFieldDescriptor specific_fields[] = {
                pdf_ignored_field("ShadingType", NULL),
                pdf_ignored_field("ColorSpace", NULL),
                pdf_ignored_field("Background", NULL),
                pdf_ignored_field("BBox", NULL),
                pdf_ignored_field("AntiAlias", NULL),
                PDF_FIELD(
                    "Coords",
                    &target_ptr->data.type3.coords,
                    PDF_DESERDE_ARRAY(
                        pdf_number_vec_push_uninit,
                        PDF_DESERDE_CUSTOM(pdf_deserde_number_trampoline)
                    )
                ),
                PDF_FIELD(
                    "Domain",
                    &target_ptr->data.type3.domain,
                    PDF_DESERDE_OPTIONAL(
                        pdf_number_vec_op_init,
                        PDF_DESERDE_ARRAY(
                            pdf_number_vec_push_uninit,
                            PDF_DESERDE_CUSTOM(pdf_deserde_number_trampoline)
                        )
                    )
                ),
                PDF_FIELD(
                    "Function",
                    &target_ptr->data.type3.function,
                    PDF_DESERDE_AS_ARRAY(
                        pdf_function_vec_push_uninit,
                        PDF_DESERDE_CUSTOM(pdf_deserde_function_trampoline)
                    )
                ),
                PDF_FIELD(
                    "Extend",
                    &target_ptr->data.type3.extend,
                    PDF_DESERDE_OPTIONAL(
                        pdf_boolean_vec_op_init,
                        PDF_DESERDE_ARRAY(
                            pdf_boolean_vec_push_uninit,
                            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_BOOLEAN)
                        )
                    )
                )
            };

            TRY(pdf_deserde_fields(
                object,
                specific_fields,
                sizeof(specific_fields) / sizeof(PdfFieldDescriptor),
                false,
                resolver,
                "Type3 shading dict"
            ));

            if (pdf_number_vec_len(target_ptr->data.type3.coords) != 6) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            if (target_ptr->data.type3.domain.has_value
                && pdf_number_vec_len(target_ptr->data.type3.domain.value)
                       != 2) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            if (target_ptr->data.type3.extend.has_value
                && pdf_boolean_vec_len(target_ptr->data.type3.extend.value)
                       != 2) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            break;
        }
        default: {
            printf(
                "%s\n",
                pdf_fmt_object(pdf_resolver_arena(resolver), object)
            );
            LOG_TODO("Type %d shading dict", target_ptr->shading_type);
        }
    }

    return NULL;
}
