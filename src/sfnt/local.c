#include "local.h"

#include "log.h"
#include "maxp.h"
#include "parser.h"
#include "pdf/result.h"

PdfResult sfnt_parse_local(
    Arena* arena,
    SfntParser* parser,
    SfntHead* head,
    SfntMaxp* maxp,
    SfntLocal* local
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(head);
    RELEASE_ASSERT(maxp);
    RELEASE_ASSERT(local);

    LOG_DEBUG_G(
        "sfnt",
        "Parsing `local` table with idx_to_loc_format=%d and num_glyphs=%u",
        head->idx_to_loc_format,
        maxp->num_glyphs
    );

    local->offsets = sfnt_uint32_array_new(arena, maxp->num_glyphs);

    if (head->idx_to_loc_format == 0) {
        // Short format
        for (size_t idx = 0; idx < maxp->num_glyphs; idx++) {
            uint16_t word_offset;
            PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &word_offset));
            sfnt_uint32_array_set(
                local->offsets,
                idx,
                (uint32_t)word_offset * 2
            );
        }
    } else {
        // Long format
        for (size_t idx = 0; idx < maxp->num_glyphs; idx++) {
            uint32_t byte_offset;
            PDF_PROPAGATE(sfnt_parser_read_uint32(parser, &byte_offset));
            sfnt_uint32_array_set(local->offsets, idx, byte_offset);
        }
    }

    return PDF_OK;
}

PdfResult
sfnt_local_glyph_offset(SfntLocal* local, uint32_t gid, uint32_t* offset) {
    RELEASE_ASSERT(local);
    RELEASE_ASSERT(offset);

    if (!sfnt_uint32_array_get(local->offsets, gid, offset)) {
        return PDF_ERR_SFNT_INVALID_GID;
    }

    return PDF_OK;
}
