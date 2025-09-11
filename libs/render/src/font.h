#pragma once

#include <stdint.h>

#include "pdf/fonts/font.h"
#include "pdf_error/error.h"

/// Returns the next CID in the data stream, advancing `offset` and setting
/// `finished` if this is the last CID in the stream.
PdfError* next_cid(
    PdfFont* font,
    PdfCMapCache* cmap_cache,
    PdfString* data,
    size_t* offset,
    bool* finished_out,
    uint32_t* cid_out
);

/// Maps a CID to a GID for the given glyph.
PdfError* cid_to_gid(
    PdfFont* font,
    PdfOptionalResolver resolver,
    uint32_t cid,
    uint32_t* gid_out
);
