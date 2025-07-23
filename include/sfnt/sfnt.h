#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "pdf/result.h"
#include "sfnt/cmap.h"

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

typedef struct SfntFont SfntFont;

PdfResult sfnt_font_new(
    Arena* arena,
    uint8_t* buffer,
    size_t buffer_len,
    SfntFont** font
);

PdfResult sfnt_font_cmap(SfntFont* font, SfntCmap* cmap);
