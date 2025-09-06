
#include "graphics_state.h"

#include "geom/mat3.h"
#include "text_state.h"

GraphicsState graphics_state_default(void) {
    return (GraphicsState
    ) {.ctm = geom_mat3_identity(),
       .text_state = text_state_default(),
       .line_width = 1.0,
       .line_cap = LINE_CAP_STYLE_BUTT,
       .line_join = LINE_JOIN_STYLE_MITER,
       .miter_limit = 10.0,
       .stroke_adjustment = false,
       .alpha_constant = 1.0,
       .alpha_source = ALPHA_SOURCE_OPACITY,
       .overprint = false,
       .overprint_mode = OVERPRINT_MODE_DEFAULT,
       .flatness = 1.0,
       .smoothness = 0.1};
}
