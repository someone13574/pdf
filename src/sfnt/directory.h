#pragma once

#include "arena/arena.h"
#include "parser.h"
#include "pdf/error.h"
#include "sfnt/sfnt.h"

PdfError* sfnt_parse_directory(
    Arena* arena,
    SfntParser* parser,
    SfntFontDirectory* font_directory
);

PdfError* sfnt_directory_get_entry(
    SfntFontDirectory* directory,
    uint32_t tag,
    SfntDirectoryEntry** entry
);
