#pragma once

#include <stdint.h>

#define DARRAY_NAME SfntUint16Array
#define DARRAY_LOWERCASE_NAME sfnt_uint16_array
#define DARRAY_TYPE uint16_t
#include "arena/darray_decl.h"

typedef struct {
    uint16_t platform_id;
    uint16_t platform_specific_id;
    uint32_t offset;
} SfntCmapHeader;

#define DVEC_NAME SfntCmapHeaderVec
#define DVEC_LOWERCASE_NAME sfnt_cmap_header_vec
#define DVEC_TYPE SfntCmapHeader
#include "arena/dvec_decl.h"

typedef struct {
    uint16_t length;
    uint16_t language;
    uint16_t seg_count_x2;
    uint16_t search_range;
    uint16_t entry_selector;
    uint16_t range_shift;
    SfntUint16Array* end_code;
    uint16_t reserved_pad;
    SfntUint16Array* start_code;
    SfntUint16Array* id_delta;
    SfntUint16Array* id_range_offset;
    SfntUint16Array* glyph_index_array;
} SfntCmapFormat4;

typedef struct {
    uint16_t format;

    union {
        SfntCmapFormat4 format4;
    } data;
} SfntCmapSubtable;

typedef struct {
    uint16_t version;
    uint16_t num_subtables;
    SfntCmapHeaderVec* headers;
} SfntCmap;

void sfnt_cmap_select_encoding(SfntCmap* cmap, size_t* encoding_idx);
