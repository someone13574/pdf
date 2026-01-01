#include "pdf/fonts/font_descriptor.h"

#include <string.h>

#include "../deser.h"
#include "err/error.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

Error* pdf_deser_font_descriptor(
    const PdfObject* object,
    PdfFontDescriptor* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Type",
            &target_ptr->type,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "FontName",
            &target_ptr->font_name,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "FontFamily",
            &target_ptr->font_family,
            PDF_DESER_OPTIONAL(
                pdf_name_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_STRING)
            )
        ),
        PDF_FIELD(
            "FontStretch",
            &target_ptr->font_stretch,
            PDF_DESER_OPTIONAL(
                pdf_name_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
            )
        ),
        PDF_FIELD(
            "FontWeight",
            &target_ptr->font_weight,
            PDF_DESER_OPTIONAL(
                pdf_name_op_init,
                PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
            )
        ),
        PDF_FIELD(
            "Flags",
            &target_ptr->flags,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(
            "FontBBox",
            &target_ptr->font_bbox,
            PDF_DESER_OPTIONAL(
                pdf_rectangle_op_init,
                PDF_DESER_CUSTOM(pdf_deser_rectangle_trampoline)
            )
        ),
        PDF_FIELD(
            "ItalicAngle",
            &target_ptr->italic_angle,
            PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
        ),
        PDF_FIELD(
            "Ascent",
            &target_ptr->ascent,
            PDF_DESER_OPTIONAL(
                pdf_number_op_init,
                PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
            )
        ),
        PDF_FIELD(
            "Descent",
            &target_ptr->descent,
            PDF_DESER_OPTIONAL(
                pdf_number_op_init,
                PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
            )
        ),
        PDF_FIELD(
            "Leading",
            &target_ptr->leading,
            PDF_DESER_OPTIONAL(
                pdf_number_op_init,
                PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
            )
        ),
        PDF_FIELD(
            "CapHeight",
            &target_ptr->cap_height,
            PDF_DESER_OPTIONAL(
                pdf_number_op_init,
                PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
            )
        ),
        PDF_FIELD(
            "XHeight",
            &target_ptr->x_height,
            PDF_DESER_OPTIONAL(
                pdf_number_op_init,
                PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
            )
        ),
        PDF_FIELD(
            "StemV",
            &target_ptr->stem_v,
            PDF_DESER_OPTIONAL(
                pdf_number_op_init,
                PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
            )
        ),
        PDF_FIELD(
            "StemH",
            &target_ptr->stem_h,
            PDF_DESER_OPTIONAL(
                pdf_number_op_init,
                PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
            )
        ),
        PDF_FIELD(
            "AvgWidth",
            &target_ptr->avg_width,
            PDF_DESER_OPTIONAL(
                pdf_number_op_init,
                PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
            )
        ),
        PDF_FIELD(
            "MaxWidth",
            &target_ptr->max_width,
            PDF_DESER_OPTIONAL(
                pdf_number_op_init,
                PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
            )
        ),
        PDF_FIELD(
            "MissingWidth",
            &target_ptr->missing_width,
            PDF_DESER_OPTIONAL(
                pdf_number_op_init,
                PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
            )
        ),
        PDF_FIELD(
            "FontFile",
            &target_ptr->font_file,
            PDF_DESER_OPTIONAL(
                pdf_stream_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_STREAM)
            )
        ),
        PDF_FIELD(
            "FontFile2",
            &target_ptr->font_file2,
            PDF_DESER_OPTIONAL(
                pdf_stream_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_STREAM)
            )
        ),
        PDF_FIELD(
            "FontFile3",
            &target_ptr->font_file3,
            PDF_DESER_OPTIONAL(
                pdf_stream_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_STREAM)
            )
        ),
        PDF_UNIMPLEMENTED_FIELD("Style"),
        PDF_FIELD(
            "Lang",
            &target_ptr->lang,
            PDF_DESER_OPTIONAL(
                pdf_name_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
            )
        ),
        PDF_UNIMPLEMENTED_FIELD("FD"),
        PDF_IGNORED_FIELD("CIDSet", &target_ptr->cid_set)
    };

    TRY(pdf_deser_dict(
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
    if (target_ptr->font_file.has_value) {
        num_font_files++;
    }
    if (target_ptr->font_file2.has_value) {
        num_font_files++;
    }
    if (target_ptr->font_file3.has_value) {
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

DESER_IMPL_RESOLVABLE(
    PdfFontDescriptorRef,
    PdfFontDescriptor,
    pdf_font_descriptor_ref_init,
    pdf_resolve_font_descriptor,
    pdf_deser_font_descriptor
)

DESER_IMPL_OPTIONAL(
    PdfFontDescriptorRefOptional,
    pdf_font_descriptor_ref_op_init
)
