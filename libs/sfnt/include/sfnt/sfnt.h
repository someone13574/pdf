#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "err/error.h"
#include "sfnt/glyph.h"
#include "sfnt/types.h"

typedef struct {
    uint32_t tag;
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;
} SfntDirectoryEntry;

#define DARRAY_NAME SfntDirectoryEntryVec
#define DARRAY_LOWERCASE_NAME sfnt_directory_entry_array
#define DARRAY_TYPE SfntDirectoryEntry
#include "arena/darray_decl.h"

typedef struct {
    uint32_t scalar_type;
    uint16_t num_tables;
    uint16_t search_range;
    uint16_t entry_selector;
    uint16_t range_shift;
    SfntDirectoryEntryVec* entries;
} SfntFontDirectory;

typedef struct {
    SfntFixed version;
    SfntFixed font_revision;
    uint32_t check_sum_adjustment;
    uint16_t flags;
    uint16_t units_per_em;
    SfntLongDateTime created;
    SfntLongDateTime modified;
    SfntFWord x_min;
    SfntFWord y_min;
    SfntFWord x_max;
    SfntFWord y_max;
    uint16_t mac_style;
    uint16_t lowest_rec_ppem;
    int16_t front_direction_hint;
    int16_t idx_to_loc_format;
    int16_t glyph_data_format;
} SfntHead;

typedef struct SfntFont SfntFont;

Error* sfnt_font_new(
    Arena* arena,
    const uint8_t* buffer,
    size_t buffer_len,
    SfntFont** font
);

SfntHead sfnt_font_head(SfntFont* font);

Error*
sfnt_get_glyph_for_cid(SfntFont* font, uint32_t cid, SfntGlyph* glyph_out);
Error*
sfnt_get_glyph_for_gid(SfntFont* font, uint32_t gid, SfntGlyph* glyph_out);
