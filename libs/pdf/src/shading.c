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
