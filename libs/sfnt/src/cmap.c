#include "sfnt/cmap.h"

#include <stddef.h>
#include <stdint.h>

#include "arena/arena.h"
#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/binary.h"
#include "parse_ctx/ctx.h"

#define DVEC_NAME SfntCmapHeaderVec
#define DVEC_LOWERCASE_NAME sfnt_cmap_header_vec
#define DVEC_TYPE SfntCmapHeader
#include "arena/dvec_impl.h"

Error*
sfnt_parse_cmap_header(Arena* arena, ParseCtx* ctx, SfntCmapHeader* subtable) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(subtable);

    TRY(parse_ctx_read_u16_be(ctx, &subtable->platform_id));
    TRY(parse_ctx_read_u16_be(ctx, &subtable->platform_specific_id));
    TRY(parse_ctx_read_u32_be(ctx, &subtable->offset));

    return NULL;
}

static Error* parse_cmap_format0(ParseCtx* ctx, SfntCmapFormat0* data) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(data);

    TRY(parse_ctx_read_u16_be(ctx, &data->length));
    TRY(parse_ctx_read_u16_be(ctx, &data->language));
    if (data->length != 262) {
        return ERROR(
            SFNT_ERR_INVALID_LENGTH,
            "Cmap format0 length must be 262"
        );
    }

    TRY(parse_ctx_new_subctx(ctx, 256, &data->glyph_index_array));
    return NULL;
}

static Error* parse_cmap_format4(ParseCtx* ctx, SfntCmapFormat4* data) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(data);

    TRY(parse_ctx_read_u16_be(ctx, &data->length));
    TRY(parse_ctx_read_u16_be(ctx, &data->language));

    TRY(parse_ctx_read_u16_be(ctx, &data->seg_count_x2));

    int num_words = 8 + 4 * (data->seg_count_x2 / 2);
    size_t num_bytes = (size_t)num_words * 2;
    size_t glyph_index_array_bytes = data->length - num_bytes;
    if ((glyph_index_array_bytes & 0x1) != 0) {
        return ERROR(
            PDF_ERR_CMAP_INVALID_GIA_LEN,
            "The glyph index array's length isn't word-aligned"
        );
    }

    uint16_t reserved_pad;
    TRY(parse_ctx_read_u16_be(ctx, &data->search_range));
    TRY(parse_ctx_read_u16_be(ctx, &data->entry_selector));
    TRY(parse_ctx_read_u16_be(ctx, &data->range_shift));
    TRY(parse_ctx_new_subctx(ctx, data->seg_count_x2, &data->end_code));
    TRY(parse_ctx_read_u16_be(ctx, &reserved_pad));
    TRY(parse_ctx_new_subctx(ctx, data->seg_count_x2, &data->start_code));
    TRY(parse_ctx_new_subctx(ctx, data->seg_count_x2, &data->id_delta));
    TRY(parse_ctx_new_subctx(ctx, data->seg_count_x2, &data->id_range_offset));
    TRY(parse_ctx_new_subctx(
        ctx,
        glyph_index_array_bytes,
        &data->glyph_index_array
    ));

    if (reserved_pad != 0) {
        return ERROR(
            SFNT_ERR_RESERVED,
            "The cmap format4 reserved pad word wasn't zero"
        );
    }

    return NULL;
}

static Error* parse_cmap_format6(ParseCtx* ctx, SfntCmapFormat6* data) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(data);

    TRY(parse_ctx_read_u16_be(ctx, &data->length));
    TRY(parse_ctx_read_u16_be(ctx, &data->language));
    TRY(parse_ctx_read_u16_be(ctx, &data->first_code));
    TRY(parse_ctx_read_u16_be(ctx, &data->entry_count));
    if (data->length != 10 + data->entry_count * 2) {
        return ERROR(SFNT_ERR_INVALID_LENGTH);
    }

    TRY(parse_ctx_new_subctx(
        ctx,
        (size_t)data->entry_count * 2,
        &data->glyph_index_array
    ));

    return NULL;
}

static Error* sfnt_cmap_get_encoding(
    SfntCmap* cmap,
    ParseCtx* ctx,
    size_t idx,
    SfntCmapSubtable* subtable
) {
    RELEASE_ASSERT(cmap);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(subtable);

    SfntCmapHeader header;
    RELEASE_ASSERT(sfnt_cmap_header_vec_get(cmap->headers, idx, &header));
    ctx->offset = header.offset;

    TRY(parse_ctx_read_u16_be(ctx, &subtable->format));

    switch (subtable->format) {
        case 0: {
            return parse_cmap_format0(ctx, &subtable->data.format0);
        }
        case 4: {
            return parse_cmap_format4(ctx, &subtable->data.format4);
        }
        case 6: {
            return parse_cmap_format6(ctx, &subtable->data.format6);
        }
        default: {
            LOG_TODO(
                "Unimplemented cmap format %u",
                (unsigned int)subtable->format
            );
        }
    }
}

/// Select the best subtable, following
/// https://github.com/harfbuzz/harfbuzz/blob/e3d0aeab7a0657e99667291ae6f75bab3455244f/src/hb-ot-cmap-table.hh#L1953
size_t sfnt_cmap_select_encoding(SfntCmap* cmap) {
    RELEASE_ASSERT(cmap);

    int best_score = -1;
    size_t selected_idx = 0;

    for (size_t idx = 0; idx < (size_t)cmap->num_subtables; idx++) {
        SfntCmapHeader subtable;
        RELEASE_ASSERT(sfnt_cmap_header_vec_get(cmap->headers, idx, &subtable));

        int score = -1;
        if (subtable.platform_id == 0) {
            switch (subtable.platform_specific_id) {
                case 0: {
                    score = 3;
                    break;
                }
                case 1: {
                    score = 4;
                    break;
                }
                case 2: {
                    score = 5;
                    break;
                }
                case 3: {
                    score = 6;
                    break;
                }
                case 4: {
                    score = 8;
                    break;
                }
                case 6: {
                    score = 9;
                    break;
                }
                default: {
                    LOG_TODO(
                        "Platform specific id %d",
                        subtable.platform_specific_id
                    );
                }
            }
        } else if (subtable.platform_id == 1) {
            switch (subtable.platform_specific_id) {
                case 0: {
                    score = 2;
                    break;
                }
                default: {
                    score = 1;
                    break;
                }
            }
        } else if (subtable.platform_id == 3) {
            switch (subtable.platform_specific_id) {
                case 0: {
                    score = 11;
                    break;
                }
                case 1: {
                    score = 7;
                    break;
                }
                case 10: {
                    score = 10;
                    break;
                }
                default: {
                    LOG_TODO(
                        "Platform specific id %d",
                        subtable.platform_specific_id
                    );
                }
            }
        } else {
            LOG_TODO("Platform id %d", subtable.platform_id);
        }

        if (score > best_score) {
            best_score = score;
            selected_idx = idx;
        }
    }

    LOG_DIAG(INFO, SFNT, "Selected cmap encoding table %zu", selected_idx);

    return selected_idx;
}

Error* sfnt_parse_cmap(Arena* arena, ParseCtx ctx, SfntCmap* cmap) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(cmap);

    TRY(parse_ctx_seek(&ctx, 0));
    TRY(parse_ctx_read_u16_be(&ctx, &cmap->version));
    TRY(parse_ctx_read_u16_be(&ctx, &cmap->num_subtables));

    cmap->headers = sfnt_cmap_header_vec_new(arena);
    for (size_t idx = 0; idx < cmap->num_subtables; idx++) {
        SfntCmapHeader header;
        TRY(sfnt_parse_cmap_header(arena, &ctx, &header));
        sfnt_cmap_header_vec_push(cmap->headers, header);
    }

    size_t encoding_idx = sfnt_cmap_select_encoding(cmap);
    TRY(sfnt_cmap_get_encoding(cmap, &ctx, encoding_idx, &cmap->mapping_table));

    return NULL;
}

static uint8_t cmap_format0_map(const SfntCmapFormat0* subtable, uint16_t cid) {
    RELEASE_ASSERT(subtable);

    if (cid >= 256) {
        LOG_WARN(
            SFNT,
            "No valid glyph mapping in format0 cmap for cid %u",
            (unsigned int)cid
        );
        return 0;
    }

    uint8_t gid = 0;
    ParseCtx data = subtable->glyph_index_array;
    REQUIRE(parse_ctx_seek(&data, (size_t)cid));
    REQUIRE(parse_ctx_read_u8(&data, &gid));
    return gid;
}

static uint16_t
cmap_format4_map(const SfntCmapFormat4* subtable, uint16_t cid) {
    RELEASE_ASSERT(subtable);

    uint16_t seg_count = subtable->seg_count_x2 / 2;
    ParseCtx start_code_data = subtable->start_code;
    ParseCtx end_code_data = subtable->end_code;

    // Find segment (TODO: binary search)
    size_t segment = SIZE_MAX;
    uint16_t start_code = 0;
    for (size_t seg_idx = 0; seg_idx < seg_count; seg_idx++) {
        uint16_t end_code;
        REQUIRE(parse_ctx_read_u16_be(&start_code_data, &start_code));
        REQUIRE(parse_ctx_read_u16_be(&end_code_data, &end_code));

        if (cid >= start_code && cid <= end_code) {
            segment = seg_idx;
            break;
        }
    }

    if (segment == SIZE_MAX) {
        LOG_WARN(SFNT, "No valid cmap format4 segment for cid %d", (int)cid);
        return 0;
    }

    // Map cid to gid
    uint16_t id_delta, id_range_offset;
    REQUIRE(parse_ctx_get_u16_be(subtable->id_delta, segment, &id_delta));
    REQUIRE(parse_ctx_get_u16_be(
        subtable->id_range_offset,
        segment,
        &id_range_offset
    ));

    if (id_range_offset == 0) {
        return (uint16_t)(cid + id_delta);
    }

    size_t word_offset = id_range_offset / 2;
    size_t idx_in_segment = cid - start_code;
    size_t glyph_idx_array_offset = seg_count - segment;

    size_t glyph_idx = word_offset + idx_in_segment - glyph_idx_array_offset;
    uint16_t uncorrected_gid;
    REQUIRE(parse_ctx_get_u16_be(
        subtable->glyph_index_array,
        glyph_idx,
        &uncorrected_gid
    ));

    if (uncorrected_gid == 0) {
        return 0;
    }

    return (uint16_t)(uncorrected_gid + id_delta);
}

static uint16_t
cmap_format6_map(const SfntCmapFormat6* subtable, uint16_t cid) {
    RELEASE_ASSERT(subtable);

    if (cid < subtable->first_code
        || cid >= subtable->first_code + subtable->entry_count) {
        LOG_WARN(
            SFNT,
            "No valid glyph mapping in format6 cmap for cid %u",
            (unsigned int)cid
        );
        return 0;
    }

    uint16_t gid = 0;
    REQUIRE(parse_ctx_get_u16_be(
        subtable->glyph_index_array,
        (size_t)(cid - subtable->first_code),
        &gid
    ));
    return gid;
}

uint32_t sfnt_cmap_map_cid(const SfntCmapSubtable* subtable, uint32_t cid) {
    RELEASE_ASSERT(subtable);

    switch (subtable->format) {
        case 0: {
            return (
                uint16_t
            )cmap_format0_map(&subtable->data.format0, (uint16_t)cid);
        }
        case 4: {
            return cmap_format4_map(&subtable->data.format4, (uint16_t)cid);
        }
        case 6: {
            return cmap_format6_map(&subtable->data.format6, (uint16_t)cid);
        }
        default: {
            LOG_TODO("Unimplemented cmap format %d", subtable->format);
        }
    }
}

#ifdef TEST
#include "test/test.h"

TEST_FUNC(test_cmap_format4_mapping) {
    static const uint8_t cmap_format4_table[] = {
        0x00, 0x04, 0x00, 0x30, 0x00, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x14, 0x00, 0x5A, 0x00, 0x99, 0xFF, 0xFF, 0x00, 0x00,
        0x00, 0x0A, 0x00, 0x1E, 0x00, 0x64, 0xFF, 0xFF, 0xFF, 0xF7, 0xFF, 0xEE,
        0xFF, 0xE5, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    ParseCtx ctx = parse_ctx_new(
        cmap_format4_table,
        sizeof(cmap_format4_table) / sizeof(uint8_t)
    );
    ctx.offset = 4;

    SfntCmapFormat4 table;
    REQUIRE(parse_cmap_format4(&ctx, &table));

    TEST_ASSERT_EQ((uint16_t)1, cmap_format4_map(&table, 10));
    TEST_ASSERT_EQ((uint16_t)11, cmap_format4_map(&table, 20));
    TEST_ASSERT_EQ((uint16_t)12, cmap_format4_map(&table, 30));
    TEST_ASSERT_EQ((uint16_t)72, cmap_format4_map(&table, 90));
    TEST_ASSERT_EQ((uint16_t)0, cmap_format4_map(&table, 21));

    return TEST_RESULT_PASS;
}

#endif // TEST
