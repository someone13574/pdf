#include "sfnt/sfnt.h"

#include "arena/arena.h"
#include "cmap.h"
#include "directory.h"
#include "glyph.h"
#include "head.h"
#include "hhea.h"
#include "hmtx.h"
#include "loca.h"
#include "logger/log.h"
#include "maxp.h"
#include "parser.h"
#include "pdf_error/error.h"

struct SfntFont {
    Arena* arena;
    SfntParser parser;
    SfntParser glyf_parser;

    SfntFontDirectory directory;
    SfntHead head;
    SfntMaxp maxp;
    SfntLoca loca;
    SfntCmap cmap;
    SfntHmtx hmtx;

    bool has_cmap;
};

PdfError* new_table_parser(SfntFont* font, uint32_t tag, SfntParser* parser) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(parser);

    LOG_DIAG(
        INFO,
        SFNT,
        "New subparser for table `%c%c%c%c`",
        (tag >> 24) & 0xff,
        (tag >> 16) & 0xff,
        (tag >> 8) & 0xff,
        tag & 0xff
    );

    SfntDirectoryEntry* entry;
    PDF_PROPAGATE(sfnt_directory_get_entry(&font->directory, tag, &entry));
    PDF_PROPAGATE(sfnt_subparser_new(
        &font->parser,
        entry->offset,
        entry->length,
        entry->checksum,
        tag == 0x68656164,
        parser
    ));

    LOG_DIAG(
        TRACE,
        SFNT,
        "Table entry: offset=%u, len=%u",
        entry->offset,
        entry->length
    );

    return NULL;
}

PdfError* sfnt_font_new(
    Arena* arena,
    uint8_t* buffer,
    size_t buffer_len,
    SfntFont** font
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(buffer);
    RELEASE_ASSERT(font);

    *font = arena_alloc(arena, sizeof(SfntFont));
    (*font)->arena = arena;
    (*font)->has_cmap = false;
    sfnt_parser_new(buffer, buffer_len, &(*font)->parser);

    PDF_PROPAGATE(
        sfnt_parse_directory(arena, &(*font)->parser, &(*font)->directory)
    );

    SfntParser head_parser;
    PDF_PROPAGATE(new_table_parser(*font, 0x68656164, &head_parser));
    PDF_PROPAGATE(sfnt_parse_head(&head_parser, &(*font)->head));

    SfntParser maxp_parser;
    PDF_PROPAGATE(new_table_parser(*font, 0x6d617870, &maxp_parser));
    PDF_PROPAGATE(sfnt_parse_maxp(&maxp_parser, &(*font)->maxp));

    SfntParser loca_parser;
    PDF_PROPAGATE(new_table_parser(*font, 0x6c6f6361, &loca_parser));
    PDF_PROPAGATE(sfnt_parse_loca(
        arena,
        &loca_parser,
        &(*font)->head,
        &(*font)->maxp,
        &(*font)->loca
    ));

    SfntParser hhea_parser;
    SfntHhea hhea;
    PDF_PROPAGATE(new_table_parser(*font, 0x68686561, &hhea_parser));
    PDF_PROPAGATE(sfnt_parse_hhea(&hhea_parser, &hhea));

    SfntParser hmtx_parser;
    PDF_PROPAGATE(new_table_parser(*font, 0x686d7478, &hmtx_parser));
    PDF_PROPAGATE(sfnt_parse_hmtx(
        arena,
        &hmtx_parser,
        &(*font)->maxp,
        &hhea,
        &(*font)->hmtx
    ));

    PDF_PROPAGATE(new_table_parser(*font, 0x676c7966, &(*font)->glyf_parser));

    return NULL;
}

SfntHead sfnt_font_head(SfntFont* font) {
    RELEASE_ASSERT(font);
    return font->head;
}

PdfError*
sfnt_get_glyph_for_cid(SfntFont* font, uint32_t cid, SfntGlyph* glyph_out) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(glyph_out);

    if (!font->has_cmap) {
        SfntParser cmap_parser;
        PDF_PROPAGATE(new_table_parser(font, 0x636d6170, &cmap_parser));
        PDF_PROPAGATE(sfnt_parse_cmap(font->arena, &cmap_parser, &font->cmap));
    }

    uint32_t gid = sfnt_cmap_map_cid(&font->cmap.mapping_table, cid);
    PDF_PROPAGATE(sfnt_get_glyph_for_gid(font, gid, glyph_out));
    return NULL;
}

PdfError*
sfnt_get_glyph_for_gid(SfntFont* font, uint32_t gid, SfntGlyph* glyph_out) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(glyph_out);

    uint32_t offset, next_offset = 0;
    PDF_PROPAGATE(sfnt_loca_glyph_offset(&font->loca, gid, &offset));
    bool next_ok = pdf_error_free_is_ok(
        sfnt_loca_glyph_offset(&font->loca, gid + 1, &next_offset)
    );
    RELEASE_ASSERT(!next_ok || next_offset >= offset);

    LOG_DIAG(
        DEBUG,
        SFNT,
        "gid=%u, offset=%u, next_offset=%u",
        gid,
        offset,
        next_offset
    );

    if (!next_ok || next_offset - offset != 0) {
        PDF_PROPAGATE(sfnt_parser_seek(&font->glyf_parser, (size_t)offset));
        PDF_PROPAGATE(
            sfnt_parse_glyph(font->arena, &font->glyf_parser, glyph_out)
        );
    } else {
        LOG_DIAG(DEBUG, SFNT, "Glyph is empty");
        glyph_out->num_contours = 0;
    }

    // TODO: Make metrics optional, since hhea.num_of_long_for_metrics isn't
    // always the number of glyphs
    SfntLongHorMetric metrics;
    RELEASE_ASSERT(
        sfnt_hmetrics_array_get(font->hmtx.h_metrics, gid, &metrics)
    );
    glyph_out->advance_width = metrics.advance_width;
    glyph_out->left_side_bearing = metrics.left_side_bearing;

    return NULL;
}
