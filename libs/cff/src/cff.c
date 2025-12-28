#include "cff/cff.h"

#include <unistd.h>

#include "arena/arena.h"
#include "charsets.h"
#include "charstring.h"
#include "geom/mat3.h"
#include "header.h"
#include "index.h"
#include "logger/log.h"
#include "parser.h"
#include "pdf_error/error.h"
#include "private_dict.h"
#include "top_dict.h"
#include "types.h"

typedef struct {
    char* name;
    CffTopDict top_dict;
    CffPrivateDict private_dict;
    CffIndex subrs_index;
    CffIndex charstr_index;
    CffCharset charset;
} CffFont;

#define DARRAY_NAME CffFontArray
#define DARRAY_LOWERCASE_NAME cff_font_array
#define DARRAY_TYPE CffFont
#include "arena/darray_impl.h"

struct CffFontSet {
    CffParser parser;

    CffHeader header;
    CffIndex name_index;
    CffIndex top_dict_index;
    CffIndex string_index;
    CffIndex global_subr_index;

    CffFontArray* fonts;
};

PdfError* cff_parse_fontset(
    Arena* arena,
    const uint8_t* data,
    size_t data_len,
    CffFontSet** cff_font_out
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(data);
    RELEASE_ASSERT(cff_font_out);

    CffFontSet fontset;
    fontset.parser = cff_parser_new(data, data_len);

    PDF_PROPAGATE(cff_read_header(&fontset.parser, &fontset.header));
    PDF_PROPAGATE(cff_parser_seek(&fontset.parser, fontset.header.header_size));
    PDF_PROPAGATE(cff_parse_index(&fontset.parser, &fontset.name_index));
    PDF_PROPAGATE(cff_parse_index(&fontset.parser, &fontset.top_dict_index));
    PDF_PROPAGATE(cff_parse_index(&fontset.parser, &fontset.string_index));
    PDF_PROPAGATE(cff_parse_index(&fontset.parser, &fontset.global_subr_index));

    fontset.fonts = cff_font_array_new_init(
        arena,
        fontset.name_index.count,
        (CffFont) {.top_dict = cff_top_dict_default(),
                   .private_dict = cff_private_dict_default()}
    );
    for (CffCard16 font_idx = 0; font_idx < fontset.name_index.count;
         font_idx++) {
        CffFont* font = NULL;
        RELEASE_ASSERT(cff_font_array_get_ptr(fontset.fonts, font_idx, &font));

        // Name
        size_t name_len;
        PDF_PROPAGATE(cff_index_seek_object(
            &fontset.name_index,
            &fontset.parser,
            font_idx,
            &name_len
        ));
        PDF_PROPAGATE(
            cff_parser_get_str(arena, &fontset.parser, name_len, &font->name)
        );
        LOG_DIAG(INFO, CFF, "Font name: %s", font->name);

        // Top dict
        size_t top_dict_len;
        PDF_PROPAGATE(cff_index_seek_object(
            &fontset.top_dict_index,
            &fontset.parser,
            font_idx,
            &top_dict_len
        ));
        PDF_PROPAGATE(
            cff_parse_top_dict(&fontset.parser, top_dict_len, &font->top_dict)
        );

        // Private dict
        CffPrivateDict private_dict = cff_private_dict_default();
        PDF_PROPAGATE(cff_parser_seek(
            &fontset.parser,
            (size_t)font->top_dict.private_offset
        ));
        PDF_PROPAGATE(cff_parse_private_dict(
            arena,
            &fontset.parser,
            (size_t)font->top_dict.private_dict_size,
            &private_dict
        ));

        // Local sub-routines
        PDF_PROPAGATE(cff_parser_seek(
            &fontset.parser,
            (size_t)font->top_dict.private_offset + (size_t)private_dict.subrs
        ));
        PDF_PROPAGATE(cff_parse_index(&fontset.parser, &font->subrs_index));

        // Char string index
        PDF_PROPAGATE(cff_parser_seek(
            &fontset.parser,
            (size_t)font->top_dict.char_strings
        ));
        PDF_PROPAGATE(cff_parse_index(&fontset.parser, &font->charstr_index));

        // Charset
        if (font->top_dict.charset <= 2) {
            LOG_TODO("Support predefined charset IDs");
        }

        PDF_PROPAGATE(
            cff_parser_seek(&fontset.parser, (size_t)font->top_dict.charset)
        );
        PDF_PROPAGATE(cff_parse_charset(
            &fontset.parser,
            arena,
            font->charstr_index.count,
            &font->charset
        ));
    }

    *cff_font_out = arena_alloc(arena, sizeof(CffFontSet));
    **cff_font_out = fontset;

    return NULL;
}

PdfError* cff_render_glyph(
    CffFontSet* fontset,
    uint32_t gid,
    Canvas* canvas,
    GeomMat3 transform,
    CanvasBrush brush
) {
    RELEASE_ASSERT(fontset);
    RELEASE_ASSERT(canvas);

    if (cff_font_array_len(fontset->fonts) != 1) {
        LOG_TODO("Fontsets");
    }

    CffFont font;
    RELEASE_ASSERT(cff_font_array_get(fontset->fonts, 0, &font));

    size_t charstr_len;
    PDF_PROPAGATE(cff_index_seek_object(
        &font.charstr_index,
        &fontset->parser,
        (CffCard16)gid,
        &charstr_len
    ));

    PDF_PROPAGATE(cff_charstr2_render(
        &fontset->parser,
        fontset->global_subr_index,
        font.subrs_index,
        charstr_len,
        canvas,
        transform,
        brush
    ));

    return NULL;
}

GeomMat3 cff_font_matrix(CffFontSet* fontset) {
    RELEASE_ASSERT(fontset);

    if (cff_font_array_len(fontset->fonts) != 1) {
        LOG_TODO("Fontsets");
    }

    CffFont font;
    RELEASE_ASSERT(cff_font_array_get(fontset->fonts, 0, &font));

    return font.top_dict.font_matrix;
}
