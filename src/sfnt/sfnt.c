#include "sfnt/sfnt.h"

#include "arena/arena.h"
#include "cmap.h"
#include "directory.h"
#include "log.h"
#include "parser.h"
#include "pdf/result.h"
#include "sfnt/cmap.h"

struct SfntFont {
    Arena* arena;
    SfntParser parser;
    SfntFontDirectory directory;
};

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

    return PDF_OK;
}

PdfResult sfnt_font_cmap(SfntFont* font, SfntCmap* cmap) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(cmap);

    SfntDirectoryEntry* entry;
    PDF_PROPAGATE(sfnt_directory_get_entry(&font->directory, 0x636d6170, &entry)
    );

    SfntParser parser;
    PDF_PROPAGATE(sfnt_subparser_new(
        &font->parser,
        entry->offset,
        entry->length,
        entry->checksum,
        &parser
    ));

    PDF_PROPAGATE(sfnt_parse_cmap(font->arena, &parser, cmap));

    size_t encoding_idx;
    sfnt_cmap_select_encoding(cmap, &encoding_idx);
    LOG_INFO_G("sfnt", "Selected cmap encoding table %zu", encoding_idx);

    PDF_PROPAGATE(sfnt_cmap_get_encoding(
        font->arena,
        cmap,
        &parser,
        encoding_idx,
        &cmap->mapping_table
    ));

    return PDF_OK;
}
