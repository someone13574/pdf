#pragma once

#include "cache.h"
#include "canvas/canvas.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "pdf/fonts/font.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

typedef enum {
    TEXT_RENDERING_MODE_FILL,
    TEXT_RENDERING_MODE_STROKE,
    TEXT_RENDERING_MODE_FILL_STROKE,
    TEXT_RENDERING_MODE_INVISIBLE,
    TEXT_RENDERING_MODE_FILL_CLIP,
    TEXT_RENDERING_MODE_STROKE_CLIP,
    TEXT_RENDERING_MODE_FILL_STROKE_CLIP,
    TEXT_RENDERING_MODE_CLIP
} TextRenderingMode;

typedef struct {
    bool font_set;

    PdfReal character_spacing;   // T_c
    PdfReal word_spacing;        // T_w
    PdfReal horizontal_scaling;  // T_h
    PdfReal leading;             // T_l
    PdfFont text_font;           // T_f
    PdfReal text_font_size;      // T_fs
    TextRenderingMode text_mode; // T_mode
    PdfReal text_rise;           // T_rise
    // text_knockout
} TextState;

TextState text_state_default(void);

typedef struct {
    GeomMat3 text_matrix;      // T_m
    GeomMat3 text_line_matrix; // T_lm
} TextObjectState;

TextObjectState text_object_state_default(void);

Error* text_state_render(
    Arena* arena,
    Canvas* canvas,
    PdfResolver* resolver,
    RenderCache* cache,
    GeomMat3 ctm,
    TextState* state,
    TextObjectState* object_state,
    PdfString text,
    CanvasBrush brush
);
