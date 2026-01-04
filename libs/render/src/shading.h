#pragma once

#include "canvas/canvas.h"
#include "geom/mat3.h"
#include "pdf/shading.h"

void render_shading(PdfShadingDict* shading_dict, GeomMat3 ctm, Canvas* canvas);
