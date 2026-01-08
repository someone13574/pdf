#include "sfnt/head.h"

#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/binary.h"
#include "parse_ctx/ctx.h"
#include "sfnt/types.h"

Error* sfnt_parse_head(ParseCtx ctx, SfntHead* head) {
    RELEASE_ASSERT(head);

    uint32_t magic_number;

    TRY(sfnt_read_fixed(&ctx, &head->version));
    TRY(sfnt_read_fixed(&ctx, &head->font_revision));
    TRY(parse_ctx_read_u32_be(&ctx, &head->check_sum_adjustment));
    TRY(parse_ctx_read_u32_be(&ctx, &magic_number));
    TRY(parse_ctx_read_u16_be(&ctx, &head->flags));
    TRY(parse_ctx_read_u16_be(&ctx, &head->units_per_em));
    TRY(sfnt_read_long_date_time(&ctx, &head->created));
    TRY(sfnt_read_long_date_time(&ctx, &head->modified));
    TRY(sfnt_read_fword(&ctx, &head->x_min));
    TRY(sfnt_read_fword(&ctx, &head->y_min));
    TRY(sfnt_read_fword(&ctx, &head->x_max));
    TRY(sfnt_read_fword(&ctx, &head->y_max));
    TRY(parse_ctx_read_u16_be(&ctx, &head->mac_style));
    TRY(parse_ctx_read_u16_be(&ctx, &head->lowest_rec_ppem));
    TRY(parse_ctx_read_i16_be(&ctx, &head->front_direction_hint));
    TRY(parse_ctx_read_i16_be(&ctx, &head->idx_to_loc_format));
    TRY(parse_ctx_read_i16_be(&ctx, &head->glyph_data_format));

    if (magic_number != 0x5f0f3cf5) {
        return ERROR(SFNT_ERR_BAD_MAGIC);
    }

    if (head->idx_to_loc_format != 0 && head->idx_to_loc_format != 1) {
        return ERROR(
            SFNT_ERR_BAD_HEAD,
            "Invalid idx_to_loc_format. Expected 0 or 1, found %u",
            head->idx_to_loc_format
        );
    }

    return NULL;
}
