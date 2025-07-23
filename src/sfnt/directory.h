#pragma once

#include "arena/arena.h"
#include "parser.h"
#include "pdf/result.h"
#include "sfnt/sfnt.h"

PdfResult sfnt_parse_directory(
    Arena* arena,
    SfntParser* parser,
    SfntFontDirectory* font_directory
);

PdfResult sfnt_directory_get_entry(
    SfntFontDirectory* directory,
    uint32_t tag,
    SfntDirectoryEntry** entry
);
