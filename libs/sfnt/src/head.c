#include "head.h"

#include "err/error.h"
#include "logger/log.h"
#include "parser.h"

Error* sfnt_parse_head(SfntParser* parser, SfntHead* head) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(head);

    uint32_t magic_number;

    TRY(sfnt_parser_read_fixed(parser, &head->version));
    TRY(sfnt_parser_read_fixed(parser, &head->font_revision));
    TRY(sfnt_parser_read_uint32(parser, &head->check_sum_adjustment));
    TRY(sfnt_parser_read_uint32(parser, &magic_number));
    TRY(sfnt_parser_read_uint16(parser, &head->flags));
    TRY(sfnt_parser_read_uint16(parser, &head->units_per_em));
    TRY(sfnt_parser_read_long_date_time(parser, &head->created));
    TRY(sfnt_parser_read_long_date_time(parser, &head->modified));
    TRY(sfnt_parser_read_fword(parser, &head->x_min));
    TRY(sfnt_parser_read_fword(parser, &head->y_min));
    TRY(sfnt_parser_read_fword(parser, &head->x_max));
    TRY(sfnt_parser_read_fword(parser, &head->y_max));
    TRY(sfnt_parser_read_uint16(parser, &head->mac_style));
    TRY(sfnt_parser_read_uint16(parser, &head->lowest_rec_ppem));
    TRY(sfnt_parser_read_int16(parser, &head->front_direction_hint));
    TRY(sfnt_parser_read_int16(parser, &head->idx_to_loc_format));
    TRY(sfnt_parser_read_int16(parser, &head->glyph_data_format));

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
