#include "sfnt/loca.h"

#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/ctx.h"

Error* sfnt_parse_loca(
    Arena* arena,
    ParseCtx ctx,
    const SfntHead* head,
    const SfntMaxp* maxp,
    SfntLoca* loca
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(head);
    RELEASE_ASSERT(maxp);
    RELEASE_ASSERT(loca);

    LOG_DIAG(
        DEBUG,
        SFNT,
        "Parsing `loca` table with idx_to_loc_format=%d and num_glyphs=%u",
        head->idx_to_loc_format,
        maxp->num_glyphs
    );

    loca->offsets = uint32_array_new(arena, maxp->num_glyphs);

    if (head->idx_to_loc_format == 0) {
        // Short format
        for (size_t idx = 0; idx < maxp->num_glyphs; idx++) {
            uint16_t word_offset;
            TRY(parse_ctx_read_u16_be(&ctx, &word_offset));
            uint32_array_set(loca->offsets, idx, (uint32_t)word_offset * 2);
        }
    } else {
        // Long format
        for (size_t idx = 0; idx < maxp->num_glyphs; idx++) {
            uint32_t byte_offset;
            TRY(parse_ctx_read_u32_be(&ctx, &byte_offset));
            uint32_array_set(loca->offsets, idx, byte_offset);
        }
    }

    return NULL;
}

Error*
sfnt_loca_glyph_offset(const SfntLoca* loca, uint32_t gid, uint32_t* offset) {
    RELEASE_ASSERT(loca);
    RELEASE_ASSERT(offset);

    if (!uint32_array_get(loca->offsets, gid, offset)) {
        return ERROR(
            SFNT_ERR_INVALID_GID,
            "Couldn't find loca entry for glyph id %u",
            gid
        );
    }

    return NULL;
}
