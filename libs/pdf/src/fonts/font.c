#include "pdf/fonts/font.h"

#include <string.h>

#include "../deser.h"
#include "err/error.h"
#include "logger/log.h"
#include "pdf/fonts/encoding.h"
#include "pdf/fonts/font_descriptor.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

DESER_IMPL_TRAMPOLINE(pdf_deser_cid_font_trampoline, pdf_deser_cid_font)

#define DVEC_NAME PdfCIDFontVec
#define DVEC_LOWERCASE_NAME pdf_cid_font_vec
#define DVEC_TYPE PdfCIDFont
#include "arena/dvec_impl.h"

Error* pdf_deser_cid_font(
    const PdfObject* object,
    PdfCIDFont* target_ptr,
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
            "Subtype",
            &target_ptr->subtype,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "BaseFont",
            &target_ptr->base_font,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "CIDSystemInfo",
            &target_ptr->cid_system_info,
            PDF_DESER_CUSTOM(pdf_deser_cid_system_info_trampoline)
        ),
        PDF_FIELD(
            "FontDescriptor",
            &target_ptr->font_descriptor,
            PDF_DESER_RESOLVABLE(pdf_font_descriptor_ref_init)
        ),
        PDF_FIELD(
            "DW",
            &target_ptr->dw,
            PDF_DESER_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "W",
            &target_ptr->w,
            PDF_DESER_OPTIONAL(
                pdf_font_widths_op_init,
                PDF_DESER_CUSTOM(pdf_deser_font_widths_trampoline)
            )
        ),
        PDF_UNIMPLEMENTED_FIELD("DW2"),
        PDF_UNIMPLEMENTED_FIELD("W2"),
        PDF_FIELD(
            "CIDToGIDMap",
            &target_ptr->cid_to_gid_map,
            PDF_DESER_OPTIONAL(
                pdf_cid_to_gid_map_op_init,
                PDF_DESER_CUSTOM(pdf_deser_cid_to_gid_map_trampoline)
            )
        )
    };

    TRY(pdf_deser_dict(
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

Error* pdf_deser_type0_font(
    const PdfObject* object,
    PdfType0font* target_ptr,
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
            "Subtype",
            &target_ptr->subtype,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "BaseFont",
            &target_ptr->base_font,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "Encoding",
            &target_ptr->encoding,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "DescendantFonts",
            &target_ptr->descendant_fonts,
            PDF_DESER_ARRAY(
                pdf_cid_font_vec_push_uninit,
                PDF_DESER_CUSTOM(pdf_deser_cid_font_trampoline)
            )
        ),
        PDF_FIELD(
            "ToUnicode",
            &target_ptr->to_unicode,
            PDF_DESER_OPTIONAL(
                pdf_stream_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_STREAM)
            )
        )
    };

    TRY(pdf_deser_dict(
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

Error* pdf_deser_truetype_font_dict(
    const PdfObject* object,
    PdfTrueTypeFont* target_ptr,
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
            "Subtype",
            &target_ptr->subtype,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "BaseFont",
            &target_ptr->base_font,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "FirstChar",
            &target_ptr->first_char,
            PDF_DESER_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "LastChar",
            &target_ptr->last_char,
            PDF_DESER_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "Widths",
            &target_ptr->widths,
            PDF_DESER_OPTIONAL(
                pdf_number_vec_op_init,
                PDF_DESER_ARRAY(
                    pdf_number_vec_push_uninit,
                    PDF_DESER_CUSTOM(pdf_deser_number_trampoline)
                )
            )
        ),
        PDF_FIELD(
            "FontDescriptor",
            &target_ptr->font_descriptor,
            PDF_DESER_OPTIONAL(
                pdf_font_descriptor_ref_op_init,
                PDF_DESER_RESOLVABLE(pdf_font_descriptor_ref_init)
            )
        ),
        PDF_FIELD(
            "Encoding",
            &target_ptr->encoding,
            PDF_DESER_OPTIONAL(
                pdf_encoding_dict_op_init,
                PDF_DESER_CUSTOM(pdf_deser_encoding_dict_trampoline)
            )
        ),
        PDF_FIELD(
            "ToUnicode",
            &target_ptr->to_unicode,
            PDF_DESER_OPTIONAL(
                pdf_stream_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_STREAM)
            )
        )
    };

    TRY(pdf_deser_dict(
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

Error* pdf_deser_font(
    const PdfObject* object,
    PdfFont* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(target_ptr);

    PdfFontInfo font_info;
    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Type",
            &font_info.type,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "Subtype",
            &font_info.subtype,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        )
    };

    TRY(pdf_deser_dict(
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
        TRY(pdf_deser_type0_font(object, &target_ptr->data.type0, resolver));
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
        TRY(pdf_deser_truetype_font_dict(
            object,
            &target_ptr->data.true_type,
            resolver
        ));
    } else if (strcmp(font_info.subtype, "CIDFontType0") == 0) {
        target_ptr->type = PDF_FONT_CIDTYPE0;
        TRY(pdf_deser_cid_font(object, &target_ptr->data.cid, resolver));
    } else if (strcmp(font_info.subtype, "CIDFontType2") == 0) {
        target_ptr->type = PDF_FONT_CIDTYPE2;
        TRY(pdf_deser_cid_font(object, &target_ptr->data.cid, resolver));
    } else {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Invalid font subtype `%s`",
            font_info.subtype
        );
    }

    return NULL;
}
