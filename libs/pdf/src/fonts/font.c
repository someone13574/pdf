#include "pdf/fonts/font.h"

#include <string.h>

#include "../deserialize.h"
#include "logger/log.h"
#include "pdf/fonts/font_descriptor.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

static PdfError* pdf_deserialize_cid_font_wrapper(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    return pdf_deserialize_cid_font(object, arena, resolver, deserialized);
}

PdfError* pdf_deserialize_type0_font(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfType0font* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfType0font,
            "Type",
            type,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            PdfType0font,
            "Subtype",
            subtype,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            PdfType0font,
            "BaseFont",
            base_font,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            PdfType0font,
            "Encoding",
            encoding,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            PdfType0font,
            "DescendantFonts",
            descendant_fonts,
            PDF_ARRAY_FIELD(
                PdfDescendantFontArray,
                PdfCIDFont,
                PDF_CUSTOM_FIELD(pdf_deserialize_cid_font_wrapper)
            )
        ),
        PDF_FIELD(
            PdfType0font,
            "ToUnicode",
            to_unicode,
            PDF_OPTIONAL_FIELD(
                PdfOpStream,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STREAM)
            )
        )
    };

    deserialized->raw_dict = object;
    PDF_PROPAGATE(pdf_deserialize_object(
        deserialized,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        resolver,
        false
    ));

    if (strcmp(deserialized->type, "Font") != 0) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect `Type` for font: %s",
            deserialized->type
        );
    }

    if (strcmp(deserialized->subtype, "Type0") != 0) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect `Subtype` for Type0 font: %s",
            deserialized->subtype
        );
    }

    return NULL;
}

PdfError* pdf_deserialize_cid_font(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfCIDFont* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfCIDFont,
            "Type",
            type,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            PdfCIDFont,
            "Subtype",
            subtype,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            PdfCIDFont,
            "BaseFont",
            base_font,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            PdfCIDFont,
            "CIDSystemInfo",
            type,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_DICT)
        ),
        PDF_FIELD(
            PdfCIDFont,
            "FontDescriptor",
            font_descriptor,
            PDF_REF_FIELD(PdfFontDescriptorRef)
        ),
        PDF_FIELD(
            PdfCIDFont,
            "DW",
            dw,
            PDF_OPTIONAL_FIELD(
                PdfOpInteger,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            PdfCIDFont,
            "W",
            w,
            PDF_OPTIONAL_FIELD(
                PdfOpFontWidths,
                PDF_CUSTOM_FIELD(pdf_deserialize_font_widths_wrapper)
            )
        )
    };

    deserialized->raw_dict = object;
    PDF_PROPAGATE(pdf_deserialize_object(
        deserialized,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        resolver,
        false
    ));

    return NULL;
}

typedef struct {
    PdfName type;
    PdfName subtype;
} PdfFontInfo;

PdfError* pdf_deserialize_font(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfFont* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfFontInfo,
            "Type",
            type,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            PdfFontInfo,
            "Subtype",
            subtype,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        )
    };

    PdfFontInfo font_info;
    PDF_PROPAGATE(pdf_deserialize_object(
        &font_info,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        resolver,
        true
    ));

    if (strcmp(font_info.type, "Font") != 0) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Expected font dictionary, found `Type=%s`",
            font_info.type
        );
    }

    if (strcmp(font_info.subtype, "Type0") == 0) {
        deserialized->type = PDF_FONT_TYPE0;
        PDF_PROPAGATE(pdf_deserialize_type0_font(
            object,
            arena,
            resolver,
            &deserialized->data.type0
        ));
    } else if (strcmp(font_info.subtype, "Type1") == 0) {
        deserialized->type = PDF_FONT_TYPE1;
        LOG_TODO("Type1 font dictionaries");
    } else if (strcmp(font_info.subtype, "MMType1") == 0) {
        deserialized->type = PDF_FONT_MMTYPE1;
        LOG_TODO("Multiple master font dictionaries");
    } else if (strcmp(font_info.subtype, "Type3") == 0) {
        deserialized->type = PDF_FONT_TYPE3;
        LOG_TODO("Type3 font dictionaries");
    } else if (strcmp(font_info.subtype, "TrueType") == 0) {
        deserialized->type = PDF_FONT_TRUETYPE;
        LOG_TODO("TrueType font dictionaries");
    } else if (strcmp(font_info.subtype, "CIDFontType0") == 0) {
        deserialized->type = PDF_FONT_CIDTYPE0;
        PDF_PROPAGATE(pdf_deserialize_cid_font(
            object,
            arena,
            resolver,
            &deserialized->data.cid
        ));
    } else if (strcmp(font_info.subtype, "CIDFontType2") == 0) {
        deserialized->type = PDF_FONT_CIDTYPE2;
        PDF_PROPAGATE(pdf_deserialize_cid_font(
            object,
            arena,
            resolver,
            &deserialized->data.cid
        ));
    } else {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Invalid font subtype `%s`",
            font_info.subtype
        );
    }

    return NULL;
}
