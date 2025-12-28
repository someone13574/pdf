#pragma once

#include "canvas/canvas.h"
#include "geom/mat3.h"
#include "index.h"
#include "parser.h"
#include "pdf_error/error.h"

PdfError* cff_charstr2_render(
    CffParser* parser,
    CffIndex global_subr_index,
    CffIndex subr_index,
    size_t length,
    Canvas* canvas,
    GeomMat3 transform,
    CanvasBrush brush
);
