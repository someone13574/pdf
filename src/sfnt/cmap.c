#include "cmap.h"

#include <stdint.h>

#include "arena/arena.h"
#include "log.h"
#include "parser.h"
#include "pdf/result.h"
#include "sfnt/cmap.h"

#define DARRAY_NAME SfntUint16Array
#define DARRAY_LOWERCASE_NAME sfnt_uint16_array
#define DARRAY_TYPE uint16_t
#include "../arena/darray_impl.h"

#define DVEC_NAME SfntCmapHeaderVec
#define DVEC_LOWERCASE_NAME sfnt_cmap_header_vec
#define DVEC_TYPE SfntCmapHeader
#include "../arena/dvec_impl.h"

PdfResult sfnt_parse_cmap_subtable(
    Arena* arena,
    SfntParser* parser,
    SfntCmapHeader* subtable
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(subtable);

    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &subtable->platform_id));
    PDF_PROPAGATE(
        sfnt_parser_read_uint16(parser, &subtable->platform_specific_id)
    );
    PDF_PROPAGATE(sfnt_parser_read_uint32(parser, &subtable->offset));

    return PDF_OK;
}

PdfResult sfnt_parse_cmap(Arena* arena, SfntParser* parser, SfntCmap* cmap) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(cmap);
    RELEASE_ASSERT(parser->offset == 0);

    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &cmap->version));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &cmap->num_subtables));

    cmap->headers = sfnt_cmap_header_vec_new(arena);
    for (size_t idx = 0; idx < cmap->num_subtables; idx++) {
        SfntCmapHeader subtable;
        PDF_PROPAGATE(sfnt_parse_cmap_subtable(arena, parser, &subtable));
        sfnt_cmap_header_vec_push(cmap->headers, subtable);
    }

    return PDF_OK;
}

void sfnt_cmap_select_encoding(SfntCmap* cmap, size_t* encoding_idx) {
    RELEASE_ASSERT(cmap);
    RELEASE_ASSERT(encoding_idx);

    bool found_unicode = false;
    bool found_non_bmp_unicode = false;

    for (size_t idx = 0; idx < (size_t)cmap->num_subtables; idx++) {
        SfntCmapHeader subtable;
        RELEASE_ASSERT(sfnt_cmap_header_vec_get(cmap->headers, idx, &subtable));

        if (subtable.platform_id == 0) {
            found_unicode = true;

            if (subtable.platform_specific_id == 3 && found_non_bmp_unicode) {
                continue;
            }

            if (subtable.platform_specific_id != 3) {
                found_non_bmp_unicode = true;
            }

            *encoding_idx = idx;
        } else if (found_unicode) {
            continue;
        }
    }
}

static PdfResult
parse_cmap_format4(Arena* arena, SfntParser* parser, SfntCmapFormat4* data) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(data);

    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &data->length));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &data->language));

    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &data->seg_count_x2));
    data->end_code = sfnt_uint16_array_new(arena, data->seg_count_x2 / 2);
    data->start_code = sfnt_uint16_array_new(arena, data->seg_count_x2 / 2);
    data->id_delta = sfnt_uint16_array_new(arena, data->seg_count_x2 / 2);
    data->id_range_offset =
        sfnt_uint16_array_new(arena, data->seg_count_x2 / 2);

    size_t num_words = 8 + 4 * (data->seg_count_x2 / 2);
    size_t num_bytes = num_words * 2;
    size_t glyph_index_array_bytes = data->length - num_bytes;
    if ((glyph_index_array_bytes & 0x1) != 0) {
        return PDF_ERR_CMAP_INVALID_GIA_LEN;
    }
    data->glyph_index_array =
        sfnt_uint16_array_new(arena, glyph_index_array_bytes / 2);

    uint16_t reserved_pad;
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &data->search_range));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &data->entry_selector));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &data->range_shift));
    PDF_PROPAGATE(sfnt_parser_read_uint16_array(parser, data->end_code));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &reserved_pad));
    PDF_PROPAGATE(sfnt_parser_read_uint16_array(parser, data->start_code));
    PDF_PROPAGATE(sfnt_parser_read_uint16_array(parser, data->id_delta));
    PDF_PROPAGATE(sfnt_parser_read_uint16_array(parser, data->id_range_offset));
    PDF_PROPAGATE(sfnt_parser_read_uint16_array(parser, data->glyph_index_array)
    );

    if (reserved_pad != 0) {
        return PDF_ERR_CMAP_RESERVED_PAD;
    }

    return PDF_OK;
}

PdfResult sfnt_cmap_get_encoding(
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

    uint16_t format;
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &format));

    switch (format) {
        case 4: {
            subtable->format = format;
            return parse_cmap_format4(arena, parser, &subtable->data.format4);
        }
        default: {
            LOG_TODO("Unimplemented cmap format %d", format);
        }
    }

    return PDF_OK;
}

uint16_t cmap_format4_map(SfntCmapFormat4* subtable, uint16_t cid) {
    RELEASE_ASSERT(subtable);

    uint16_t seg_count = subtable->seg_count_x2 / 2;

    // Find segment (TODO: binary search)
    size_t segment = SIZE_MAX;
    uint16_t start_code = 0;
    for (size_t seg_idx = 0; seg_idx < seg_count; seg_idx++) {
        uint16_t end_code;
        RELEASE_ASSERT(
            sfnt_uint16_array_get(subtable->start_code, seg_idx, &start_code)
        );
        RELEASE_ASSERT(
            sfnt_uint16_array_get(subtable->end_code, seg_idx, &end_code)
        );

        if (cid >= start_code && cid <= end_code) {
            segment = seg_idx;
            break;
        }
    }

    if (segment == SIZE_MAX) {
        LOG_WARN_G(
            "sfnt",
            "No valid cmap format4 segment for cid %d",
            (int)cid
        );
        return 0;
    }

    // Map cid to gid
    uint16_t id_delta, id_range_offset;
    RELEASE_ASSERT(sfnt_uint16_array_get(subtable->id_delta, segment, &id_delta)
    );
    RELEASE_ASSERT(sfnt_uint16_array_get(
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
    RELEASE_ASSERT(sfnt_uint16_array_get(
        subtable->glyph_index_array,
        glyph_idx,
        &uncorrected_gid
    ));

    if (uncorrected_gid == 0) {
        return 0;
    }

    return (uint16_t)(uncorrected_gid + id_delta);
}

uint32_t sfnt_cmap_map_cid(SfntCmapSubtable* subtable, uint32_t cid) {
    RELEASE_ASSERT(subtable);

    switch (subtable->format) {
        case 4: {
            return cmap_format4_map(&subtable->data.format4, (uint16_t)cid);
        }
        default: {
            LOG_TODO("Unimplemented cmap format %d", subtable->format);
        }
    }
}

#ifdef TEST
#include "test.h"

TEST_FUNC(test_cmap_format4_mapping) {
    Arena* arena = arena_new(1024);
    SfntCmapFormat4 table = {
        .seg_count_x2 = 8,
        .end_code = sfnt_uint16_array_new_from(
            arena,
            4,
            (uint16_t[4]) {20, 90, 153, 0xffff}
        ),
        .start_code = sfnt_uint16_array_new_from(
            arena,
            4,
            (uint16_t[4]) {10, 30, 100, 0xffff}
        ),
        .id_delta = sfnt_uint16_array_new_from(
            arena,
            4,
            (uint16_t[4]) {(uint16_t)-9, (uint16_t)-18, (uint16_t)-27, 1}
        ),
        .id_range_offset = sfnt_uint16_array_new_init(arena, 4, 0)
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
