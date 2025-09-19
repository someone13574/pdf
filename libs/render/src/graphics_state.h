#pragma once

#include "pdf/object.h"
#include "text_state.h"

typedef enum {
    LINE_CAP_STYLE_BUTT,
    LINE_CAP_STYLE_ROUND,
    LINE_CAP_STYLE_PROJECTING
} LineCapStyle;

typedef enum {
    LINE_JOIN_STYLE_MITER,
    LINE_JOIN_STYLE_ROUND,
    LINE_JOIN_STYLE_BEVEL
} LineJoinStyle;

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
    LineCapStyle line_cap;
    LineJoinStyle line_join;
    PdfReal miter_limit;
    // dash_pattern
    // rendering_intent
    bool stroke_adjustment;
    // blend_mode
    // soft_mask
    double alpha_constant;
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

#define DLINKED_NAME GraphicsStateStack
#define DLINKED_LOWERCASE_NAME graphics_state_stack
#define DLINKED_TYPE GraphicsState
#include "arena/dlinked_decl.h"
