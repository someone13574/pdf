#include "sfnt/sfnt.h"

#include "arena/arena.h"
#include "checksum.h"
#include "directory.h"
#include "err/error.h"
#include "glyph.h"
#include "logger/log.h"
#include "parse_ctx/ctx.h"

Error* new_table_parser(SfntFont* font, uint32_t tag, ParseCtx* out) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(out);

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
    TRY(sfnt_directory_get_entry(&font->directory, tag, &entry));
    TRY(parse_ctx_seek(&font->ctx, entry->offset));
    TRY(parse_ctx_new_subctx(&font->ctx, entry->length, out));
    TRY(sfnt_validate_checksum(*out, entry->checksum, tag == 0x68656164));

    LOG_DIAG(
        TRACE,
        SFNT,
        "Table entry: offset=%u, len=%u",
        entry->offset,
        entry->length
    );

    return NULL;
}

Error* sfnt_font_new(Arena* arena, ParseCtx ctx, SfntFont* out) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(out);

    out->arena = arena;
    out->has_cmap = false;
    out->ctx = ctx;

    TRY(sfnt_parse_directory(arena, ctx, &out->directory));

    ParseCtx head_ctx;
    TRY(new_table_parser(out, 0x68656164, &head_ctx));
    TRY(sfnt_parse_head(head_ctx, &out->head));

    ParseCtx maxp_ctx;
    TRY(new_table_parser(out, 0x6d617870, &maxp_ctx));
    TRY(sfnt_parse_maxp(maxp_ctx, &out->maxp));

    ParseCtx loca_parser;
    TRY(new_table_parser(out, 0x6c6f6361, &loca_parser));
    TRY(
        sfnt_parse_loca(arena, loca_parser, &out->head, &out->maxp, &out->loca)
    );

    ParseCtx hhea_parser;
    SfntHhea hhea;
    TRY(new_table_parser(out, 0x68686561, &hhea_parser));
    TRY(sfnt_parse_hhea(hhea_parser, &hhea));

    ParseCtx hmtx_parser;
    TRY(new_table_parser(out, 0x686d7478, &hmtx_parser));
    TRY(sfnt_parse_hmtx(arena, hmtx_parser, &out->maxp, &hhea, &out->hmtx));

    TRY(new_table_parser(out, 0x676c7966, &out->glyf_parser));

    return NULL;
}

SfntHead sfnt_font_head(SfntFont* font) {
    RELEASE_ASSERT(font);
    return font->head;
}

Error*
sfnt_get_glyph_for_cid(SfntFont* font, uint32_t cid, SfntGlyph* glyph_out) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(glyph_out);

    if (!font->has_cmap) {
        ParseCtx cmap_parser;
        TRY(new_table_parser(font, 0x636d6170, &cmap_parser));
        TRY(sfnt_parse_cmap(font->arena, cmap_parser, &font->cmap));
    }

    uint32_t gid = sfnt_cmap_map_cid(&font->cmap.mapping_table, cid);
    TRY(sfnt_get_glyph_for_gid(font, gid, glyph_out));
    return NULL;
}

Error*
sfnt_get_glyph_for_gid(SfntFont* font, uint32_t gid, SfntGlyph* glyph_out) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(glyph_out);

    uint32_t offset, next_offset = 0;
    TRY(sfnt_loca_glyph_offset(&font->loca, gid, &offset));
    bool next_ok = error_free_is_ok(
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
        TRY(parse_ctx_seek(&font->glyf_parser, (size_t)offset));
        TRY(sfnt_parse_glyph(font->arena, font->glyf_parser, glyph_out));
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
