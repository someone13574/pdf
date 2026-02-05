#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "parse_ctx/ctx.h"

/// A CFF FontSet.
typedef struct CffFontSet CffFontSet;

/// Parse a CFF FontSet, storing the result in `cff_fontset_out`.
Error*
cff_parse_fontset(Arena* arena, ParseCtx ctx, CffFontSet** cff_fontset_out);

/// Render a glyph with a given transformation.
Error* cff_render_glyph(
    CffFontSet* fontset,
    uint32_t gid,
    Canvas* canvas,
    GeomMat3 transform,
    CanvasBrush brush
);

GeomMat3 cff_font_matrix(CffFontSet* fontset);
