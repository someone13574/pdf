#pragma once

#include "canvas/canvas.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "index.h"
#include "parser.h"

Error* cff_charstr2_render(
    CffParser* parser,
    CffIndex global_subr_index,
    CffIndex subr_index,
    size_t length,
    Canvas* canvas,
    GeomMat3 transform,
    CanvasBrush brush
);
