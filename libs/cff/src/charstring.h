#pragma once

#include "canvas/canvas.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "index.h"
#include "parse_ctx/ctx.h"

Error* cff_charstr2_render(
    ParseCtx* ctx,
    CffIndex global_subr_index,
    CffIndex local_subr_index,
    size_t length,
    Canvas* canvas,
    GeomMat3 transform,
    CanvasBrush brush
);
