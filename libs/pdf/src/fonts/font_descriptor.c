#include "pdf/fonts/font_descriptor.h"

#include <string.h>

#include "../deserialize.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/types.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_font_descriptor(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfFontDescriptor* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfFontDescriptor,
            "Type",
            type,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "FontName",
            font_name,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "FontFamily",
            font_family,
            PDF_OPTIONAL_FIELD(
                PdfOpName,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "FontStretch",
            font_stretch,
            PDF_OPTIONAL_FIELD(
                PdfOpName,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "FontWeight",
            font_weight,
            PDF_OPTIONAL_FIELD(
                PdfOpNumber,
                PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "Flags",
            flags,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "FontBBox",
            font_bbox,
            PDF_OPTIONAL_FIELD(
                PdfOpRectangle,
                PDF_CUSTOM_FIELD(pdf_deserialize_rectangle_wrapper)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "ItalicAngle",
            italic_angle,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "Ascent",
            ascent,
            PDF_OPTIONAL_FIELD(
                PdfOpNumber,
                PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "Descent",
            descent,
            PDF_OPTIONAL_FIELD(
                PdfOpNumber,
                PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "Leading",
            leading,
            PDF_OPTIONAL_FIELD(
                PdfOpNumber,
                PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "CapHeight",
            cap_height,
            PDF_OPTIONAL_FIELD(
                PdfOpNumber,
                PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "StemV",
            stem_v,
            PDF_OPTIONAL_FIELD(
                PdfOpNumber,
                PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "StemH",
            stem_h,
            PDF_OPTIONAL_FIELD(
                PdfOpNumber,
                PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "AvgWidth",
            avg_width,
            PDF_OPTIONAL_FIELD(
                PdfOpNumber,
                PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "MaxWidth",
            max_width,
            PDF_OPTIONAL_FIELD(
                PdfOpNumber,
                PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "MissingWidth",
            missing_width,
            PDF_OPTIONAL_FIELD(
                PdfOpNumber,
                PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "FontFile",
            font_file,
            PDF_OPTIONAL_FIELD(
                PdfOpStream,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STREAM)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "FontFile2",
            font_file2,
            PDF_OPTIONAL_FIELD(
                PdfOpStream,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STREAM)
            )
        ),
        PDF_FIELD(
            PdfFontDescriptor,
            "FontFile3",
            font_file3,
            PDF_OPTIONAL_FIELD(
                PdfOpStream,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STREAM)
            )
        )
    };

    deserialized->raw_dict = object;
    PDF_PROPAGATE(pdf_deserialize_dict(
        deserialized,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        resolver,
        false,
        "PdfFontDescriptor"
    ));

    if (strcmp(deserialized->type, "FontDescriptor") != 0) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "`Type` must be `FontDescriptor`"
        );
    }

    size_t num_font_files = 0;
    if (deserialized->font_file.discriminant) {
        num_font_files++;
    }
    if (deserialized->font_file2.discriminant) {
        num_font_files++;
    }
    if (deserialized->font_file3.discriminant) {
        num_font_files++;
    }

    if (num_font_files > 1) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "At most, only one of the FontFile, FontFile2, and FontFile3 entries shall be present."
        );
    }

    return NULL;
}

PDF_DESERIALIZABLE_REF_IMPL(
    PdfFontDescriptor,
    font_descriptor,
    pdf_deserialize_font_descriptor
)
