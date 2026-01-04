#include "pdf/resources.h"

#include <string.h>

#include "err/error.h"
#include "logger/log.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/types.h"

Error* pdf_deserde_resources(
    const PdfObject* object,
    PdfResources* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_dict_optional_field("ExtGState", &target_ptr->ext_gstate),
        pdf_dict_optional_field("ColorSpace", &target_ptr->color_space),
        pdf_dict_optional_field("Pattern", &target_ptr->pattern),
        pdf_dict_optional_field("Shading", &target_ptr->shading),
        pdf_dict_optional_field("XObject", &target_ptr->xobject),
        pdf_dict_optional_field("Font", &target_ptr->font),
        pdf_name_vec_optional_field("ProcSet", &target_ptr->proc_set),
        pdf_dict_optional_field("Properties", &target_ptr->properties)
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfResources"
    ));

    return NULL;
}

PDF_IMPL_OPTIONAL_FIELD(PdfResources, PdfResourcesOptional, resources)

Error* pdf_deserde_gstate_params(
    const PdfObject* object,
    PdfGStateParams* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_name_optional_field("Type", &target_ptr->type),
        pdf_unimplemented_field("LW"),
        pdf_unimplemented_field("LC"),
        pdf_unimplemented_field("LJ"),
        pdf_unimplemented_field("ML"),
        pdf_unimplemented_field("D"),
        pdf_unimplemented_field("RI"),
        pdf_boolean_optional_field("OP", &target_ptr->overprint_upper),
        pdf_boolean_optional_field("op", &target_ptr->overprint_lower),
        pdf_integer_optional_field("OPM", &target_ptr->overprint_mode),
        pdf_unimplemented_field("Font"),
        pdf_unimplemented_field("BG"),
        pdf_unimplemented_field("BG2"),
        pdf_unimplemented_field("UCR"),
        pdf_unimplemented_field("UCR2"),
        pdf_ignored_field("TR", &target_ptr->tr), // TODO: functions
        pdf_unimplemented_field("TR2"),
        pdf_unimplemented_field("HT"),
        pdf_unimplemented_field("FL"),
        pdf_num_as_real_optional_field("SM", &target_ptr->sm),
        pdf_boolean_optional_field("SA", &target_ptr->sa),
        pdf_ignored_field("BM", &target_ptr->bm),       // TODO: blend mode
        pdf_ignored_field("SMask", &target_ptr->smask), // TODO: masking
        pdf_num_as_real_optional_field("CA", &target_ptr->ca_stroking),
        pdf_num_as_real_optional_field("ca", &target_ptr->ca_nonstroking),
        pdf_boolean_optional_field("AIS", &target_ptr->ais),
        pdf_unimplemented_field("TK")
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfGStateParams"
    ));

    if (target_ptr->type.is_some) {
        if (strcmp(target_ptr->type.value, "ExtGState") != 0) {
            return ERROR(PDF_ERR_INVALID_SUBTYPE, "`Type` must be `ExtGState`");
        }
    }

    return NULL;
}
