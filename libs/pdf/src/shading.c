#include "pdf/shading.h"

#include <stdio.h>

#include "err/error.h"
#include "logger/log.h"
#include "pdf/color_space.h"
#include "pdf/function.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/types.h"

Error* pdf_deserde_shading_dict(
    const PdfObject* object,
    PdfShadingDict* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_integer_field("ShadingType", &target_ptr->shading_type),
        pdf_color_space_field("ColorSpace", &target_ptr->color_space),
        pdf_unimplemented_field("Background"),
        pdf_rectangle_optional_field("BBox", &target_ptr->bbox),
        pdf_boolean_optional_field("AntiAlias", &target_ptr->anti_alias)
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
                pdf_number_vec_field("Coords", &target_ptr->data.type2.coords),
                pdf_number_vec_optional_field(
                    "Domain",
                    &target_ptr->data.type2.domain
                ),
                pdf_as_function_vec_field(
                    "Function",
                    &target_ptr->data.type2.function
                ),
                pdf_boolean_vec_optional_field(
                    "Extend",
                    &target_ptr->data.type2.extend
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

            if (target_ptr->data.type2.domain.is_some
                && pdf_number_vec_len(target_ptr->data.type2.domain.value)
                       != 2) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            if (target_ptr->data.type2.extend.is_some
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
                pdf_number_vec_field("Coords", &target_ptr->data.type3.coords),
                pdf_number_vec_optional_field(
                    "Domain",
                    &target_ptr->data.type3.domain
                ),
                pdf_as_function_vec_field(
                    "Function",
                    &target_ptr->data.type3.function
                ),
                pdf_boolean_vec_optional_field(
                    "Extend",
                    &target_ptr->data.type3.extend
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

            if (target_ptr->data.type3.domain.is_some
                && pdf_number_vec_len(target_ptr->data.type3.domain.value)
                       != 2) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            if (target_ptr->data.type3.extend.is_some
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
