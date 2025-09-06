#pragma once

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "raster_canvas.h"

Canvas* canvas_from_raster(Arena* arena, RasterCanvas* raster_canvas);
