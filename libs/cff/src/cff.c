#include "cff/cff.h"

#include "arena/arena.h"
#include "charsets.h"
#include "header.h"
#include "index.h"
#include "logger/log.h"
#include "parser.h"
#include "pdf_error/error.h"
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

        CffIndex char_string_index;
        PDF_PROPAGATE(cff_parser_seek(&parser, (size_t)top_dict.char_strings)
        ); // TODO: check offsets for neg
        PDF_PROPAGATE(cff_parse_index(&parser, &char_string_index));

        CffCharset charset = {0};
        PDF_PROPAGATE(cff_parser_seek(&parser, (size_t)top_dict.charset));
        PDF_PROPAGATE(
            cff_parse_charset(&parser, arena, char_string_index.count, &charset)
        );

        LOG_DIAG(INFO, CFF, "Done");
    }

    *cff_font_out = arena_alloc(arena, sizeof(CffFont));
    **cff_font_out = font;

    LOG_TODO();

    return NULL;
}
