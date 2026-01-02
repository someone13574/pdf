#include "pdf/fonts/font_descriptor.h"

#include <string.h>

#include "err/error.h"
#include "logger/log.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/types.h"

Error* pdf_deserde_font_descriptor(
    const PdfObject* object,
    PdfFontDescriptor* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_name_field("Type", &target_ptr->type),
        pdf_name_field("FontName", &target_ptr->font_name),
        pdf_string_optional_field("FontFamily", &target_ptr->font_family),
        pdf_name_optional_field("FontStretch", &target_ptr->font_stretch),
        pdf_number_optional_field("FontWeight", &target_ptr->font_weight),
        pdf_integer_field("Flags", &target_ptr->flags),
        pdf_rectangle_optional_field("FontBBox", &target_ptr->font_bbox),
        pdf_number_field("ItalicsAngle", &target_ptr->italic_angle),
        pdf_number_optional_field("Ascent", &target_ptr->ascent),
        pdf_number_optional_field("Descent", &target_ptr->descent),
        pdf_number_optional_field("Leading", &target_ptr->leading),
        pdf_number_optional_field("CapHeight", &target_ptr->cap_height),
        pdf_number_optional_field("XHeight", &target_ptr->x_height),
        pdf_number_optional_field("StemV", &target_ptr->stem_v),
        pdf_number_optional_field("StemH", &target_ptr->stem_h),
        pdf_number_optional_field("AvgWidth", &target_ptr->avg_width),
        pdf_number_optional_field("MaxWidth", &target_ptr->max_width),
        pdf_number_optional_field("MissingWidth", &target_ptr->missing_width),
        pdf_stream_optional_field("FontFile", &target_ptr->font_file),
        pdf_stream_optional_field("FontFile2", &target_ptr->font_file2),
        pdf_stream_optional_field("FontFile3", &target_ptr->font_file3),
        pdf_unimplemented_field("Style"),
        pdf_name_optional_field("Lang", &target_ptr->lang),
        pdf_unimplemented_field("FD"),
        pdf_ignored_field("CIDSet", &target_ptr->cid_set)
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfFontDescriptor"
    ));

    if (strcmp(target_ptr->type, "FontDescriptor") != 0) {
        return ERROR(PDF_ERR_INCORRECT_TYPE, "`Type` must be `FontDescriptor`");
    }

    size_t num_font_files = 0;
    if (target_ptr->font_file.is_some) {
        num_font_files++;
    }
    if (target_ptr->font_file2.is_some) {
        num_font_files++;
    }
    if (target_ptr->font_file3.is_some) {
        num_font_files++;
    }

    if (num_font_files > 1) {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "At most, only one of the FontFile, FontFile2, and FontFile3 entries shall be present."
        );
    }

    return NULL;
}
