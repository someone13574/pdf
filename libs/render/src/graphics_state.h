#pragma once

#include "pdf/content_stream/operation.h"
#include "pdf/object.h"
#include "pdf/resources.h"
#include "text_state.h"

typedef enum {
    /// true
    ALPHA_SOURCE_SHAPE,
    /// false
    ALPHA_SOURCE_OPACITY
} AlphaSource;

typedef enum { OVERPRINT_MODE_DEFAULT, OVERPRINT_MODE_NONZERO } OverprintMode;

typedef struct {
    GeomMat3 ctm;
    // clipping_path
    // color_space
    // color
    TextState text_state;
    PdfReal line_width;
    PdfLineCapStyle line_cap;
    PdfLineJoinStyle line_join;
    PdfReal miter_limit;
    // dash_pattern
    // rendering_intent
    bool stroke_adjustment;
    // blend_mode
    // soft_mask
    double stroking_alpha;
    double nonstroking_alpha;
    AlphaSource alpha_source;
    bool overprint;
    OverprintMode overprint_mode;
    // black_generation
    // undercolor_removal
    // transfer
    // halftone
    PdfReal flatness;
    PdfReal smoothness;
} GraphicsState;

GraphicsState graphics_state_default(void);
void graphics_state_apply_params(GraphicsState* gstate, PdfGStateParams params);

#define DLINKED_NAME GraphicsStateStack
#define DLINKED_LOWERCASE_NAME graphics_state_stack
#define DLINKED_TYPE GraphicsState
#include "arena/dlinked_decl.h"
