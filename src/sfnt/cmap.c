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

PdfResult
sfnt_cmap_get_encoding(SfntCmap* cmap, SfntParser* parser, size_t idx) {
    RELEASE_ASSERT(cmap);
    RELEASE_ASSERT(parser);

    SfntCmapHeader subtable;
    RELEASE_ASSERT(sfnt_cmap_header_vec_get(cmap->headers, idx, &subtable));
    parser->offset = subtable.offset;

    uint16_t format;
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &format));

    switch (format) {
        default: {
            LOG_TODO("Unimplemented cmap format %d", format);
        }
    }

    return PDF_OK;
}
