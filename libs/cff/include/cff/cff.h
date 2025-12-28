#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "geom/mat3.h"
#include "pdf_error/error.h"

/// A CFF FontSet.
typedef struct CffFontSet CffFontSet;

/// Parse a CFF FontSet, storing the result in `cff_fontset_out`.
PdfError* cff_parse_fontset(
    Arena* arena,
    const uint8_t* data,
    size_t data_len,
    CffFontSet** cff_fontset_out
);

/// Render a glyph with a given transformation.
PdfError* cff_render_glyph(
    CffFontSet* fontset,
    uint32_t gid,
    Canvas* canvas,
    GeomMat3 transform,
    CanvasBrush brush
);

GeomMat3 cff_font_matrix(CffFontSet* fontset);
