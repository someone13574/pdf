#include "pdf/shading.h"

#include <stdio.h>

#include "deserialize.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_shading_dict(
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
            PDF_DESERDE_CUSTOM(pdf_deserialize_color_space_trampoline)
        ),
        PDF_UNIMPLEMENTED_FIELD("Background"),
        PDF_FIELD(
            "BBox",
            &target_ptr->bbox,
            PDF_DESERDE_OPTIONAL(
                pdf_rectangle_op_init,
                PDF_DESERDE_CUSTOM(pdf_deserialize_rectangle_trampoline)
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

    PDF_PROPAGATE(pdf_deserialize_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        true,
        resolver,
        "PdfShaderDict"
    ));

    switch (target_ptr->shading_type) {
        case 3: {
            PdfFieldDescriptor specific_fields[] = {
                PDF_IGNORED_FIELD("ShadingType", NULL),
                PDF_IGNORED_FIELD("ColorSpace", NULL),
                PDF_IGNORED_FIELD("Background", NULL),
                PDF_IGNORED_FIELD("BBox", NULL),
                PDF_IGNORED_FIELD("AntiAlias", NULL),
                PDF_FIELD(
                    "Coords",
                    &target_ptr->data.type3.coords,
                    PDF_DESERDE_ARRAY(
                        pdf_number_vec_push_uninit,
                        PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
                    )
                ),
                PDF_FIELD(
                    "Domain",
                    &target_ptr->data.type3.domain,
                    PDF_DESERDE_OPTIONAL(
                        pdf_number_vec_op_init,
                        PDF_DESERDE_ARRAY(
                            pdf_number_vec_push_uninit,
                            PDF_DESERDE_CUSTOM(
                                pdf_deserialize_number_trampoline
                            )
                        )
                    )
                ),
                PDF_FIELD(
                    "Function",
                    &target_ptr->data.type3.function,
                    PDF_DESERDE_AS_ARRAY(
                        pdf_function_vec_push_uninit,
                        PDF_DESERDE_CUSTOM(pdf_deserialize_function_trampoline)
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

            PDF_PROPAGATE(pdf_deserialize_dict(
                object,
                specific_fields,
                sizeof(specific_fields) / sizeof(PdfFieldDescriptor),
                false,
                resolver,
                "Type3 shading dict"
            ));

            if (pdf_number_vec_len(target_ptr->data.type3.coords) != 6) {
                return PDF_ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            if (target_ptr->data.type3.domain.has_value
                && pdf_number_vec_len(target_ptr->data.type3.domain.value)
                       != 2) {
                return PDF_ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            if (target_ptr->data.type3.extend.has_value
                && pdf_boolean_vec_len(target_ptr->data.type3.extend.value)
                       != 2) {
                return PDF_ERROR(PDF_ERR_INCORRECT_TYPE);
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
