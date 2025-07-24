#include "sfnt/sfnt.h"

#include "arena/arena.h"
#include "cmap.h"
#include "directory.h"
#include "glyph.h"
#include "head.h"
#include "local.h"
#include "log.h"
#include "maxp.h"
#include "parser.h"
#include "pdf/result.h"

struct SfntFont {
    Arena* arena;
    SfntParser parser;
    SfntParser glyf_parser;

    SfntFontDirectory directory;
    SfntHead head;
    SfntMaxp maxp;
    SfntLocal local;
    SfntCmap cmap;
};

PdfResult new_table_parser(SfntFont* font, uint32_t tag, SfntParser* parser) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(parser);

    LOG_INFO_G(
        "sfnt",
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

    LOG_TRACE_G(
        "sfnt",
        "Table entry: offset=%u, len=%u",
        entry->offset,
        entry->length
    );

    return PDF_OK;
}

PdfResult sfnt_font_new(
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
    sfnt_parser_new(buffer, buffer_len, &(*font)->parser);

    PDF_PROPAGATE(
        sfnt_parse_directory(arena, &(*font)->parser, &(*font)->directory)
    );

    SfntParser head_parser;
    PDF_PROPAGATE(new_table_parser(*font, 0x68656164, &head_parser));
    PDF_PROPAGATE(sfnt_parse_head(&head_parser, &(*font)->head));

    SfntParser cmap_parser;
    PDF_PROPAGATE(new_table_parser(*font, 0x636d6170, &cmap_parser));
    PDF_PROPAGATE(sfnt_parse_cmap(arena, &cmap_parser, &(*font)->cmap));

    SfntParser maxp_parser;
    PDF_PROPAGATE(new_table_parser(*font, 0x6d617870, &maxp_parser));
    PDF_PROPAGATE(sfnt_parse_maxp(&maxp_parser, &(*font)->maxp));

    SfntParser local_parser;
    PDF_PROPAGATE(new_table_parser(*font, 0x6c6f6361, &local_parser));
    PDF_PROPAGATE(sfnt_parse_local(
        arena,
        &local_parser,
        &(*font)->head,
        &(*font)->maxp,
        &(*font)->local
    ));

    PDF_PROPAGATE(new_table_parser(*font, 0x676c7966, &(*font)->glyf_parser));

    return PDF_OK;
}

PdfResult sfnt_get_glyph(SfntFont* font, uint32_t cid, SfntGlyph* glyph) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(glyph);

    uint32_t gid = sfnt_cmap_map_cid(&font->cmap.mapping_table, cid);
    uint32_t offset;
    PDF_PROPAGATE(sfnt_local_glyph_offset(&font->local, gid, &offset));

    LOG_DEBUG_G("sfnt", "cid=%u, gid=%u, offset=%u", cid, gid, offset);

    PDF_PROPAGATE(sfnt_parser_seek(&font->glyf_parser, (size_t)offset));
    PDF_PROPAGATE(sfnt_parse_glyph(font->arena, &font->glyf_parser, glyph));

    return PDF_OK;
}
