#pragma once

#include <stdint.h>

#include "cache.h"
#include "canvas/canvas.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "pdf/fonts/font.h"
#include "pdf/resolver.h"

// TODO: Ideally this would be entirely within the PDF library

/// Returns the next CID in the data stream, advancing `offset` and setting
/// `finished` if this is the last CID in the stream.
Error* next_cid(
    PdfFont* font,
    RenderCache* cache,
    PdfString* data,
    size_t* offset,
    bool* finished_out,
    uint32_t* cid_out
);

/// Maps a CID to a GID for the given glyph.
Error* cid_to_gid(
    Arena* arena,
    PdfFont* font,
    RenderCache* cache,
    PdfResolver* resolver,
    uint32_t cid,
    uint32_t* gid_out
);

/// Renders a given glyph.
Error* render_glyph(
    Arena* arena,
    PdfFont* font,
    PdfResolver* resolver,
    uint32_t gid,
    Canvas* canvas,
    GeomMat3 transform,
    CanvasBrush brush
);

/// Gets the width for a CID.
Error* cid_to_width(
    PdfFont* font,
    PdfResolver* resolver,
    uint32_t cid,
    PdfNumber* width_out
);

/// Get the font matrix for a font
Error* get_font_matrix(
    Arena* arena,
    PdfResolver* resolver,
    PdfFont* font,
    GeomMat3* font_matrix_out
);
