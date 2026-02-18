#pragma once

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "geom/mat3.h"
#include "pdf/shading.h"

void render_shading(
    PdfShadingDict* shading_dict,
    Arena* arena,
    GeomMat3 ctm,
    Canvas* canvas
);
