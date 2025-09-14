#include "cff/cff.h"

#include <unistd.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
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

struct CffFont {
    CffHeader header;
    CffIndex name_index;
    CffIndex top_dict_index;
    CffIndex string_index;
    CffIndex global_subr_index;
};

PdfError* cff_parse(
    Arena* arena,
    const uint8_t* data,
    size_t data_len,
    CffFont** cff_font_out
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(data);
    RELEASE_ASSERT(cff_font_out);

    CffFont font;
    CffParser parser = cff_parser_new(data, data_len);

    PDF_PROPAGATE(cff_read_header(&parser, &font.header));
    PDF_PROPAGATE(cff_parser_seek(&parser, font.header.header_size));

    PDF_PROPAGATE(cff_parse_index(&parser, &font.name_index));
    PDF_PROPAGATE(cff_parse_index(&parser, &font.top_dict_index));
    PDF_PROPAGATE(cff_parse_index(&parser, &font.string_index));
    PDF_PROPAGATE(cff_parse_index(&parser, &font.global_subr_index));

    for (CffCard16 font_idx = 0; font_idx < font.name_index.count; font_idx++) {
        size_t name_len;
        PDF_PROPAGATE(cff_index_seek_object(
            &font.name_index,
            &parser,
            font_idx,
            &name_len
        ));

        char* name = NULL;
        PDF_PROPAGATE(cff_parser_get_str(arena, &parser, name_len, &name));
        LOG_DIAG(INFO, CFF, "Font name: %s", name);

        size_t top_dict_len;
        PDF_PROPAGATE(cff_index_seek_object(
            &font.top_dict_index,
            &parser,
            font_idx,
            &top_dict_len
        ));

        CffTopDict top_dict = cff_top_dict_default();
        PDF_PROPAGATE(cff_parse_top_dict(&parser, top_dict_len, &top_dict));

        CffPrivateDict private_dict = cff_private_dict_default();
        PDF_PROPAGATE(cff_parser_seek(&parser, (size_t)top_dict.private_offset)
        );
        PDF_PROPAGATE(cff_parse_private_dict(
            arena,
            &parser,
            (size_t)top_dict.private_dict_size,
            &private_dict
        ));

        CffIndex subrs_index;
        PDF_PROPAGATE(cff_parser_seek(
            &parser,
            (size_t)top_dict.private_offset + (size_t)private_dict.subrs
        ));
        PDF_PROPAGATE(cff_parse_index(&parser, &subrs_index));

        CffIndex charstring_index;
        PDF_PROPAGATE(cff_parser_seek(&parser, (size_t)top_dict.char_strings));
        PDF_PROPAGATE(cff_parse_index(&parser, &charstring_index));

        CffCharset charset = {0};
        if (top_dict.charset <= 2) {
            LOG_TODO("Support predefined charset IDs");
        }

        PDF_PROPAGATE(cff_parser_seek(&parser, (size_t)top_dict.charset));
        PDF_PROPAGATE(
            cff_parse_charset(&parser, arena, charstring_index.count, &charset)
        );

        for (CffCard16 idx = 0; idx < charstring_index.count; idx++) {
            size_t charstring_size;
            PDF_PROPAGATE(cff_index_seek_object(
                &charstring_index,
                &parser,
                idx,
                &charstring_size
            ));

            Canvas* canvas = canvas_new_scalable(arena, 1500, 1500, 0xffffffff);
            GeomMat3 transform = geom_mat3_new(
                1.0,
                0.0,
                0.0,
                0.0,
                -1.0,
                0.0,
                500.0,
                1000.0,
                0.0
            );
            PDF_PROPAGATE(cff_charstr2_render(
                &parser,
                font.global_subr_index,
                subrs_index,
                charstring_size,
                canvas,
                transform
            ));
            RELEASE_ASSERT(canvas_write_file(canvas, "glyph.svg"));

            usleep(150000);
        }
    }

    *cff_font_out = arena_alloc(arena, sizeof(CffFont));
    **cff_font_out = font;

    return NULL;
}
