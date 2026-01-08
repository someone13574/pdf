#include "sfnt/maxp.h"

#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/binary.h"
#include "parse_ctx/ctx.h"
#include "sfnt/types.h"

Error* sfnt_parse_maxp(ParseCtx ctx, SfntMaxp* maxp) {
    RELEASE_ASSERT(maxp);

    TRY(sfnt_read_fixed(&ctx, &maxp->version));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->num_glyphs));

    if (maxp->version == 0x5000) {
        return NULL;
    } else if (maxp->version != 0x10000) {
        return ERROR(
            SFNT_ERR_INVALID_VERSION,
            "Invalid maxp version %u",
            maxp->version
        );
    }

    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_points));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_contours));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_component_points));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_component_contours));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_zones));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_twilight_points));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_storage));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_function_defs));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_instruction_defs));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_stack_elements));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_size_of_instructions));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_component_elements));
    TRY(parse_ctx_read_u16_be(&ctx, &maxp->max_component_depth));

    return NULL;
}
