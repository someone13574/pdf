
#include "graphics_state.h"

#include "geom/mat3.h"
#include "geom/vec3.h"
#include "logger/log.h"
#include "pdf/color_space.h"
#include "text_state.h"

#define DLINKED_NAME GraphicsStateStack
#define DLINKED_LOWERCASE_NAME graphics_state_stack
#define DLINKED_TYPE GraphicsState
#include "arena/dlinked_impl.h"

GraphicsState graphics_state_default(void) {
    return (GraphicsState) {
        .ctm = geom_mat3_identity(),
        .stroking_color_space =
            (PdfColorSpace) {.family = PDF_COLOR_SPACE_DEVICE_GRAY},
        .nonstroking_color_space =
            (PdfColorSpace) {.family = PDF_COLOR_SPACE_DEVICE_GRAY},
        .stroking_rgb = geom_vec3_new(0.0, 0.0, 0.0),
        .nonstroking_rgb = geom_vec3_new(0.0, 0.0, 0.0),
        .text_state = text_state_default(),
        .line_width = 1.0,
        .line_cap = PDF_LINE_CAP_STYLE_BUTT,
        .line_join = PDF_LINE_JOIN_STYLE_MITER,
        .miter_limit = 10.0,
        .stroke_adjustment = false,
        .stroking_alpha = 1.0,
        .nonstroking_alpha = 1.0,
        .alpha_source = ALPHA_SOURCE_OPACITY,
        .stroking_overprint = false,
        .nonstroking_overprint = false,
        .overprint_mode = OVERPRINT_MODE_DEFAULT,
        .flatness = 1.0,
        .smoothness = 0.1
    };
}

void graphics_state_apply_params(
    GraphicsState* gstate,
    PdfGStateParams params
) {
    RELEASE_ASSERT(gstate);

    if (params.overprint_upper.has_value) {
        gstate->stroking_overprint = params.overprint_upper.has_value;
        gstate->nonstroking_overprint = params.overprint_upper.has_value;
    }

    if (params.overprint_lower.has_value) {
        gstate->nonstroking_overprint = params.overprint_lower.has_value;
    }

    if (params.overprint_mode.has_value) {
        gstate->overprint_mode = params.overprint_mode.value == 0
                                   ? OVERPRINT_MODE_DEFAULT
                                   : OVERPRINT_MODE_NONZERO;
    }

    if (params.sm.has_value) {
        gstate->smoothness = params.sm.value;
    }

    if (params.sa.has_value) {
        gstate->stroke_adjustment = params.sa.value;
    }

    if (params.ca_stroking.has_value) {
        gstate->stroking_alpha = params.ca_stroking.value;
    }

    if (params.ca_nonstroking.has_value) {
        gstate->nonstroking_alpha = params.ca_nonstroking.value;
    }

    if (params.ais.has_value) {
        gstate->alpha_source =
            params.ais.value ? ALPHA_SOURCE_SHAPE : ALPHA_SOURCE_OPACITY;
    }
}
