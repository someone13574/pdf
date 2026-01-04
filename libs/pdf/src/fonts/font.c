#include "pdf/fonts/font.h"

#include <string.h>

#include "err/error.h"
#include "logger/log.h"
#include "pdf/deserde.h"
#include "pdf/fonts/cid_to_gid_map.h"
#include "pdf/fonts/cmap.h"
#include "pdf/fonts/encoding.h"
#include "pdf/fonts/font_descriptor.h"
#include "pdf/fonts/font_widths.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/types.h"

Error* pdf_deserde_cid_font(
    const PdfObject* object,
    PdfCIDFont* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_name_field("Type", &target_ptr->type),
        pdf_name_field("Subtype", &target_ptr->subtype),
        pdf_name_field("BaseFont", &target_ptr->base_font),
        pdf_cid_system_info_field(
            "CIDSystemInfo",
            &target_ptr->cid_system_info
        ),
        pdf_font_descriptor_ref_field(
            "FontDescriptor",
            &target_ptr->font_descriptor
        ),
        pdf_integer_optional_field("DW", &target_ptr->dw),
        pdf_font_widths_optional_field("W", &target_ptr->w),
        pdf_unimplemented_field("DW2"),
        pdf_unimplemented_field("W2"),
        pdf_cid_to_gid_map_optional_field(
            "CIDToGIDMap",
            &target_ptr->cid_to_gid_map
        )
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfCIDFont"
    ));

    if (strcmp(target_ptr->type, "Font") != 0) {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "`Type` key must be `Font`, found `%s`",
            target_ptr->type
        );
    } else if (strcmp(target_ptr->subtype, "CIDFontType0") != 0
               && strcmp(target_ptr->subtype, "CIDFontType2") != 0) {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "`Subtype` key must be `CIDFontType0` or `CIDFontType2`"
        );
    }

    return NULL;
}

PDF_IMPL_FIELD(PdfCIDFont, cid_font)

#define DVEC_NAME PdfCIDFontVec
#define DVEC_LOWERCASE_NAME pdf_cid_font_vec
#define DVEC_TYPE PdfCIDFont
#include "arena/dvec_impl.h"

PDF_IMPL_ARRAY_FIELD(PdfCIDFontVec, cid_font_vec, cid_font)

Error* pdf_deserde_type0_font(
    const PdfObject* object,
    PdfType0font* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_name_field("Type", &target_ptr->type),
        pdf_name_field("Subtype", &target_ptr->subtype),
        pdf_name_field("BaseFont", &target_ptr->base_font),
        pdf_name_field("Encoding", &target_ptr->encoding),
        pdf_cid_font_vec_field(
            "DescendantFonts",
            &target_ptr->descendant_fonts
        ),
        pdf_stream_optional_field("ToUnicode", &target_ptr->to_unicode)
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfType0font"
    ));

    if (strcmp(target_ptr->type, "Font") != 0) {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect `Type` for font: %s",
            target_ptr->type
        );
    }

    if (strcmp(target_ptr->subtype, "Type0") != 0) {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect `Subtype` for Type0 font: %s",
            target_ptr->subtype
        );
    }

    if (pdf_cid_font_vec_len(target_ptr->descendant_fonts) != 1) {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "The `DescendantFonts` array of a Type0 font must have exactly one element"
        );
    }

    return NULL;
}

Error* pdf_deserde_truetype_font_dict(
    const PdfObject* object,
    PdfTrueTypeFont* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_name_field("Type", &target_ptr->type),
        pdf_name_field("Subtype", &target_ptr->subtype),
        pdf_name_field("BaseFont", &target_ptr->base_font),
        pdf_integer_optional_field("FirstChar", &target_ptr->first_char),
        pdf_integer_optional_field("LastChar", &target_ptr->last_char),
        pdf_number_vec_optional_field("Widths", &target_ptr->widths),
        pdf_font_descriptor_ref_optional_field(
            "FontDescriptor",
            &target_ptr->font_descriptor
        ),
        pdf_encoding_dict_optional_field("Encoding", &target_ptr->encoding),
        pdf_stream_optional_field("ToUnicode", &target_ptr->to_unicode)
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfTrueTypeFont"
    ));

    if (strcmp(target_ptr->type, "Font") != 0) {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "`Type` key must be `Font`, found `%s`",
            target_ptr->type
        );
    } else if (strcmp(target_ptr->subtype, "TrueType") != 0) {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "`Subtype` key must be `TrueType`"
        );
    }

    return NULL;
}

typedef struct {
    PdfName type;
    PdfName subtype;
} PdfFontInfo;

Error* pdf_deserde_font(
    const PdfObject* object,
    PdfFont* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(target_ptr);

    PdfFontInfo font_info;
    PdfFieldDescriptor fields[] = {
        pdf_name_field("Type", &font_info.type),
        pdf_name_field("Subtype", &font_info.subtype)
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        true,
        resolver,
        "PdfFontInfo"
    ));

    if (strcmp(font_info.type, "Font") != 0) {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Expected font dictionary, found `Type=%s`",
            font_info.type
        );
    }

    if (strcmp(font_info.subtype, "Type0") == 0) {
        target_ptr->type = PDF_FONT_TYPE0;
        TRY(pdf_deserde_type0_font(object, &target_ptr->data.type0, resolver));
    } else if (strcmp(font_info.subtype, "Type1") == 0) {
        target_ptr->type = PDF_FONT_TYPE1;
        LOG_TODO("Type1 font dictionaries");
    } else if (strcmp(font_info.subtype, "MMType1") == 0) {
        target_ptr->type = PDF_FONT_MMTYPE1;
        LOG_TODO("Multiple master font dictionaries");
    } else if (strcmp(font_info.subtype, "Type3") == 0) {
        target_ptr->type = PDF_FONT_TYPE3;
        LOG_TODO("Type3 font dictionaries");
    } else if (strcmp(font_info.subtype, "TrueType") == 0) {
        target_ptr->type = PDF_FONT_TRUETYPE;
        TRY(pdf_deserde_truetype_font_dict(
            object,
            &target_ptr->data.true_type,
            resolver
        ));
    } else if (strcmp(font_info.subtype, "CIDFontType0") == 0) {
        target_ptr->type = PDF_FONT_CIDTYPE0;
        TRY(pdf_deserde_cid_font(object, &target_ptr->data.cid, resolver));
    } else if (strcmp(font_info.subtype, "CIDFontType2") == 0) {
        target_ptr->type = PDF_FONT_CIDTYPE2;
        TRY(pdf_deserde_cid_font(object, &target_ptr->data.cid, resolver));
    } else {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Invalid font subtype `%s`",
            font_info.subtype
        );
    }

    return NULL;
}
