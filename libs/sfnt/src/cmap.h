#pragma once

#include "arena/arena.h"
#include "parser.h"

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
    Uint16Array* end_code;
    Uint16Array* start_code;
    Uint16Array* id_delta;
    Uint16Array* id_range_offset;
    Uint16Array* glyph_index_array;
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
    SfntCmapSubtable mapping_table;
} SfntCmap;

PdfError* sfnt_parse_cmap(Arena* arena, SfntParser* parser, SfntCmap* cmap);
uint32_t sfnt_cmap_map_cid(const SfntCmapSubtable* subtable, uint32_t cid);
