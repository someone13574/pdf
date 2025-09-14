#pragma once

#include "canvas/canvas.h"
#include "geom/mat3.h"
#include "parser.h"
#include "pdf_error/error.h"

PdfError* cff_charstr2_render(
    CffParser* parser,
    size_t length,
    Canvas* canvas,
    GeomMat3 transform
);
