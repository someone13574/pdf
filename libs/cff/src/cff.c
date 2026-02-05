#include "cff/cff.h"

#include <unistd.h>

#include "arena/arena.h"
#include "charsets.h"
#include "charstring.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "header.h"
#include "index.h"
#include "logger/log.h"
#include "parse_ctx/ctx.h"
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
    ParseCtx ctx;

    CffHeader header;
    CffIndex name_index;
    CffIndex top_dict_index;
    CffIndex string_index;
    CffIndex global_subr_index;

    CffFontArray* fonts;
};

Error* cff_parse_fontset(Arena* arena, ParseCtx ctx, CffFontSet** out) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(out);

    CffFontSet fontset;
    fontset.ctx = ctx;

    TRY(cff_read_header(&fontset.ctx, &fontset.header));
    TRY(parse_ctx_seek(&fontset.ctx, fontset.header.header_size));
    TRY(cff_parse_index(&fontset.ctx, &fontset.name_index));
    TRY(cff_parse_index(&fontset.ctx, &fontset.top_dict_index));
    TRY(cff_parse_index(&fontset.ctx, &fontset.string_index));
    TRY(cff_parse_index(&fontset.ctx, &fontset.global_subr_index));

    fontset.fonts = cff_font_array_new_init(
        arena,
        fontset.name_index.count,
        (CffFont) {.top_dict = cff_top_dict_default(),
                   .private_dict = cff_private_dict_default()}
    );
    for (uint16_t font_idx = 0; font_idx < fontset.name_index.count;
         font_idx++) {
        CffFont* font = NULL;
        RELEASE_ASSERT(cff_font_array_get_ptr(fontset.fonts, font_idx, &font));

        // Name
        size_t name_len;
        TRY(cff_index_seek_object(
            &fontset.name_index,
            &fontset.ctx,
            font_idx,
            &name_len
        ));
        TRY(cff_get_str(arena, &fontset.ctx, name_len, &font->name));
        LOG_DIAG(INFO, CFF, "Font name: %s", font->name);

        // Top dict
        size_t top_dict_len;
        TRY(cff_index_seek_object(
            &fontset.top_dict_index,
            &fontset.ctx,
            font_idx,
            &top_dict_len
        ));
        TRY(cff_parse_top_dict(&fontset.ctx, top_dict_len, &font->top_dict));

        // Private dict
        CffPrivateDict private_dict = cff_private_dict_default();
        TRY(
            parse_ctx_seek(&fontset.ctx, (size_t)font->top_dict.private_offset)
        );
        TRY(cff_parse_private_dict(
            arena,
            &fontset.ctx,
            (size_t)font->top_dict.private_dict_size,
            &private_dict
        ));

        // Local sub-routines
        TRY(parse_ctx_seek(
            &fontset.ctx,
            (size_t)font->top_dict.private_offset + (size_t)private_dict.subrs
        ));
        TRY(cff_parse_index(&fontset.ctx, &font->subrs_index));

        // Char string index
        TRY(parse_ctx_seek(&fontset.ctx, (size_t)font->top_dict.char_strings));
        TRY(cff_parse_index(&fontset.ctx, &font->charstr_index));

        // Charset
        if (font->top_dict.charset <= 2) {
            LOG_TODO("Support predefined charset IDs");
        }

        TRY(parse_ctx_seek(&fontset.ctx, (size_t)font->top_dict.charset));
        TRY(cff_parse_charset(
            &fontset.ctx,
            arena,
            font->charstr_index.count,
            &font->charset
        ));
    }

    *out = arena_alloc(arena, sizeof(CffFontSet));
    **out = fontset;

    return NULL;
}

Error* cff_render_glyph(
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
    TRY(cff_index_seek_object(
        &font.charstr_index,
        &fontset->ctx,
        (uint16_t)gid,
        &charstr_len
    ));

    TRY(cff_charstr2_render(
        &fontset->ctx,
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
