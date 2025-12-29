#pragma once

#include "pdf/fonts/agl.h"
#include "pdf/fonts/cmap.h"

typedef struct {
    PdfCMapCache* cmap_cache;
    PdfAglGlyphList* glyph_list;
} RenderCache;
