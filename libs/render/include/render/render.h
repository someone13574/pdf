#pragma once

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "err/error.h"
#include "pdf/page.h"
#include "pdf/resolver.h"

typedef enum RenderCanvasType {
    RENDER_CANVAS_TYPE_RASTER,
    RENDER_CANVAS_TYPE_SCALABLE
} RenderCanvasType;

Error* render_page(
    Arena* arena,
    PdfResolver* resolver,
    const PdfPage* page,
    RenderCanvasType canvas_type,
    Canvas** canvas
);
