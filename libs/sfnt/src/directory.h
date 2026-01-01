#pragma once

#include "arena/arena.h"
#include "err/error.h"
#include "parser.h"
#include "sfnt/sfnt.h"

Error* sfnt_parse_directory(
    Arena* arena,
    SfntParser* parser,
    SfntFontDirectory* font_directory
);

Error* sfnt_directory_get_entry(
    const SfntFontDirectory* directory,
    uint32_t tag,
    SfntDirectoryEntry** entry
);
