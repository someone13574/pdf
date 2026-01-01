#include "cmap.h"

#include <stdint.h>

#include "arena/arena.h"
#include "arena/common.h"
#include "err/error.h"
#include "logger/log.h"
#include "parser.h"

#define DVEC_NAME SfntCmapHeaderVec
#define DVEC_LOWERCASE_NAME sfnt_cmap_header_vec
#define DVEC_TYPE SfntCmapHeader
#include "arena/dvec_impl.h"

static size_t sfnt_cmap_select_encoding(SfntCmap* cmap);
static Error*
parse_cmap_format4(Arena* arena, SfntParser* parser, SfntCmapFormat4* data);
static Error* sfnt_cmap_get_encoding(
    Arena* arena,
    SfntCmap* cmap,
    SfntParser* parser,
    size_t idx,
    SfntCmapSubtable* subtable
);

Error* sfnt_parse_cmap_header(
    Arena* arena,
    SfntParser* parser,
    SfntCmapHeader* subtable
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(subtable);

    TRY(sfnt_parser_read_uint16(parser, &subtable->platform_id));
    TRY(sfnt_parser_read_uint16(parser, &subtable->platform_specific_id));
    TRY(sfnt_parser_read_uint32(parser, &subtable->offset));

    return NULL;
}

Error* sfnt_parse_cmap(Arena* arena, SfntParser* parser, SfntCmap* cmap) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(cmap);
    RELEASE_ASSERT(parser->offset == 0);

    TRY(sfnt_parser_read_uint16(parser, &cmap->version));
    TRY(sfnt_parser_read_uint16(parser, &cmap->num_subtables));

    cmap->headers = sfnt_cmap_header_vec_new(arena);
    for (size_t idx = 0; idx < cmap->num_subtables; idx++) {
        SfntCmapHeader header;
        TRY(sfnt_parse_cmap_header(arena, parser, &header));
        sfnt_cmap_header_vec_push(cmap->headers, header);
    }

    size_t encoding_idx = sfnt_cmap_select_encoding(cmap);
    TRY(sfnt_cmap_get_encoding(
        arena,
        cmap,
        parser,
        encoding_idx,
        &cmap->mapping_table
    ));

    return NULL;
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

static Error*
parse_cmap_format0(Arena* arena, SfntParser* parser, SfntCmapFormat0* data) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(data);

    TRY(sfnt_parser_read_uint16(parser, &data->length));
    TRY(sfnt_parser_read_uint16(parser, &data->language));
    if (data->length != 262) {
        return ERROR(
            SFNT_ERR_INVALID_LENGTH,
            "Cmap format0 length must be 262"
        );
    }

    data->glyph_index_array = uint8_array_new(arena, 256);
    TRY(sfnt_parser_read_uint8_array(parser, data->glyph_index_array));

    return NULL;
}

static Error*
parse_cmap_format4(Arena* arena, SfntParser* parser, SfntCmapFormat4* data) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(data);

    TRY(sfnt_parser_read_uint16(parser, &data->length));
    TRY(sfnt_parser_read_uint16(parser, &data->language));

    TRY(sfnt_parser_read_uint16(parser, &data->seg_count_x2));
    data->end_code = uint16_array_new(arena, data->seg_count_x2 / 2);
    data->start_code = uint16_array_new(arena, data->seg_count_x2 / 2);
    data->id_delta = uint16_array_new(arena, data->seg_count_x2 / 2);
    data->id_range_offset = uint16_array_new(arena, data->seg_count_x2 / 2);

    int num_words = 8 + 4 * (data->seg_count_x2 / 2);
    size_t num_bytes = (size_t)num_words * 2;
    size_t glyph_index_array_bytes = data->length - num_bytes;
    if ((glyph_index_array_bytes & 0x1) != 0) {
        return ERROR(
            PDF_ERR_CMAP_INVALID_GIA_LEN,
            "The glyph index array's length isn't word-aligned"
        );
    }
    data->glyph_index_array =
        uint16_array_new(arena, glyph_index_array_bytes / 2);

    uint16_t reserved_pad;
    TRY(sfnt_parser_read_uint16(parser, &data->search_range));
    TRY(sfnt_parser_read_uint16(parser, &data->entry_selector));
    TRY(sfnt_parser_read_uint16(parser, &data->range_shift));
    TRY(sfnt_parser_read_uint16_array(parser, data->end_code));
    TRY(sfnt_parser_read_uint16(parser, &reserved_pad));
    TRY(sfnt_parser_read_uint16_array(parser, data->start_code));
    TRY(sfnt_parser_read_uint16_array(parser, data->id_delta));
    TRY(sfnt_parser_read_uint16_array(parser, data->id_range_offset));
    TRY(sfnt_parser_read_uint16_array(parser, data->glyph_index_array));

    if (reserved_pad != 0) {
        return ERROR(
            SFNT_ERR_RESERVED,
            "The cmap format4 reserved pad word wasn't zero"
        );
    }

    return NULL;
}

static Error*
parse_cmap_format6(Arena* arena, SfntParser* parser, SfntCmapFormat6* data) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(data);

    TRY(sfnt_parser_read_uint16(parser, &data->length));
    TRY(sfnt_parser_read_uint16(parser, &data->language));
    TRY(sfnt_parser_read_uint16(parser, &data->first_code));
    TRY(sfnt_parser_read_uint16(parser, &data->entry_count));
    if (data->length != 10 + data->entry_count * 2) {
        return ERROR(SFNT_ERR_INVALID_LENGTH);
    }

    data->glyph_index_array = uint16_array_new(arena, data->entry_count);
    TRY(sfnt_parser_read_uint16_array(parser, data->glyph_index_array));

    return NULL;
}

Error* sfnt_cmap_get_encoding(
    Arena* arena,
    SfntCmap* cmap,
    SfntParser* parser,
    size_t idx,
    SfntCmapSubtable* subtable
) {
    RELEASE_ASSERT(cmap);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(subtable);

    SfntCmapHeader header;
    RELEASE_ASSERT(sfnt_cmap_header_vec_get(cmap->headers, idx, &header));
    parser->offset = header.offset;

    TRY(sfnt_parser_read_uint16(parser, &subtable->format));

    switch (subtable->format) {
        case 0: {
            return parse_cmap_format0(arena, parser, &subtable->data.format0);
        }
        case 4: {
            return parse_cmap_format4(arena, parser, &subtable->data.format4);
        }
        case 6: {
            return parse_cmap_format6(arena, parser, &subtable->data.format6);
        }
        default: {
            LOG_TODO(
                "Unimplemented cmap format %u",
                (unsigned int)subtable->format
            );
        }
    }
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
    RELEASE_ASSERT(
        uint8_array_get(subtable->glyph_index_array, (size_t)cid, &gid)
    );
    return gid;
}

static uint16_t
cmap_format4_map(const SfntCmapFormat4* subtable, uint16_t cid) {
    RELEASE_ASSERT(subtable);

    uint16_t seg_count = subtable->seg_count_x2 / 2;

    // Find segment (TODO: binary search)
    size_t segment = SIZE_MAX;
    uint16_t start_code = 0;
    for (size_t seg_idx = 0; seg_idx < seg_count; seg_idx++) {
        uint16_t end_code;
        RELEASE_ASSERT(
            uint16_array_get(subtable->start_code, seg_idx, &start_code)
        );
        RELEASE_ASSERT(
            uint16_array_get(subtable->end_code, seg_idx, &end_code)
        );

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
    RELEASE_ASSERT(uint16_array_get(subtable->id_delta, segment, &id_delta));
    RELEASE_ASSERT(
        uint16_array_get(subtable->id_range_offset, segment, &id_range_offset)
    );

    if (id_range_offset == 0) {
        return (uint16_t)(cid + id_delta);
    }

    size_t word_offset = id_range_offset / 2;
    size_t idx_in_segment = cid - start_code;
    size_t glyph_idx_array_offset = seg_count - segment;

    size_t glyph_idx = word_offset + idx_in_segment - glyph_idx_array_offset;
    uint16_t uncorrected_gid;
    RELEASE_ASSERT(uint16_array_get(
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
    RELEASE_ASSERT(uint16_array_get(
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
    Arena* arena = arena_new(1024);
    SfntCmapFormat4 table = {
        .seg_count_x2 = 8,
        .end_code = uint16_array_new_from(
            arena,
            (uint16_t[4]) {20, 90, 153, 0xffff},
            4
        ),
        .start_code = uint16_array_new_from(
            arena,
            (uint16_t[4]) {10, 30, 100, 0xffff},
            4
        ),
        .id_delta = uint16_array_new_from(
            arena,
            (uint16_t[4]) {(uint16_t)-9, (uint16_t)-18, (uint16_t)-27, 1},
            4
        ),
        .id_range_offset = uint16_array_new_init(arena, 4, 0)
    };

    TEST_ASSERT_EQ((uint16_t)1, cmap_format4_map(&table, 10));
    TEST_ASSERT_EQ((uint16_t)11, cmap_format4_map(&table, 20));
    TEST_ASSERT_EQ((uint16_t)12, cmap_format4_map(&table, 30));
    TEST_ASSERT_EQ((uint16_t)72, cmap_format4_map(&table, 90));
    TEST_ASSERT_EQ((uint16_t)0, cmap_format4_map(&table, 21));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
