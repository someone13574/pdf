#pragma once

#include "color/icc_cache.h"
#include "pdf/fonts/agl.h"
#include "pdf/fonts/cmap.h"

typedef struct {
    PdfCMapCache* cmap_cache;
    PdfAglGlyphList* glyph_list;
    IccProfileCache icc_cache;
} RenderCache;
