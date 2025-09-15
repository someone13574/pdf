#pragma once

#include <stdint.h>

#include "canvas/canvas.h"
#include "pdf/fonts/font.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

// TODO: Ideally this would be entirely within the PDF library

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

/// Renders a given glyph.
PdfError* render_glyph(
    PdfFont* font,
    PdfOptionalResolver resolver,
    uint32_t gid,
    Canvas* canvas,
    GeomMat3 transform
);

/// Gets the width for a CID.
PdfError* cid_to_width(PdfFont* font, uint32_t cid, PdfInteger* width_out);
