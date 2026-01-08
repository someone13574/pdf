#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "err/error.h"
#include "parse_ctx/ctx.h"
#include "sfnt/cmap.h"
#include "sfnt/glyph.h"
#include "sfnt/head.h"
#include "sfnt/hmtx.h"
#include "sfnt/loca.h"
#include "sfnt/maxp.h"

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
    Arena* arena;
    ParseCtx ctx;
    ParseCtx glyf_parser;

    SfntFontDirectory directory;
    SfntHead head;
    SfntMaxp maxp;
    SfntLoca loca;
    SfntCmap cmap;
    SfntHmtx hmtx;

    bool has_cmap;
} SfntFont;

Error* sfnt_font_new(Arena* arena, ParseCtx ctx, SfntFont* out);

SfntHead sfnt_font_head(SfntFont* font);

Error*
sfnt_get_glyph_for_cid(SfntFont* font, uint32_t cid, SfntGlyph* glyph_out);
Error*
sfnt_get_glyph_for_gid(SfntFont* font, uint32_t gid, SfntGlyph* glyph_out);
