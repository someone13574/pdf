
#include "graphics_state.h"

#include "geom/mat3.h"
#include "logger/log.h"
#include "text_state.h"

#define DLINKED_NAME GraphicsStateStack
#define DLINKED_LOWERCASE_NAME graphics_state_stack
#define DLINKED_TYPE GraphicsState
#include "arena/dlinked_impl.h"

GraphicsState graphics_state_default(void) {
    return (GraphicsState) {
        .ctm = geom_mat3_identity(),
        .stroking_rgb = (PdfOpParamsSetRGB) {.r = 0.0, .g = 0.0, .b = 0.0},
        .nonstroking_rgb = (PdfOpParamsSetRGB) {.r = 0.0, .g = 0.0, .b = 0.0},
        .text_state = text_state_default(),
        .line_width = 1.0,
        .line_cap = PDF_LINE_CAP_STYLE_BUTT,
        .line_join = PDF_LINE_JOIN_STYLE_MITER,
        .miter_limit = 10.0,
        .stroke_adjustment = false,
        .stroking_alpha = 1.0,
        .nonstroking_alpha = 1.0,
        .alpha_source = ALPHA_SOURCE_OPACITY,
        .overprint = false,
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

    LOG_WARN(RENDER, "TODO: Apply params");

    if (params.ca_stroking.has_value) {
        gstate->stroking_alpha = params.ca_stroking.value;
    }

    if (params.ca_nonstroking.has_value) {
        gstate->nonstroking_alpha = params.ca_nonstroking.value;
    }
}
