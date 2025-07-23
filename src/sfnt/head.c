#include "head.h"

#include "log.h"
#include "parser.h"
#include "pdf/result.h"

PdfResult sfnt_parse_head(SfntParser* parser, SfntHead* head) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(head);

    uint32_t magic_number;

    PDF_PROPAGATE(sfnt_parser_read_fixed(parser, &head->version));
    PDF_PROPAGATE(sfnt_parser_read_fixed(parser, &head->font_revision));
    PDF_PROPAGATE(sfnt_parser_read_uint32(parser, &head->check_sum_adjustment));
    PDF_PROPAGATE(sfnt_parser_read_uint32(parser, &magic_number));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &head->flags));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &head->units_per_em));
    PDF_PROPAGATE(sfnt_parser_read_long_date_time(parser, &head->created));
    PDF_PROPAGATE(sfnt_parser_read_long_date_time(parser, &head->modified));
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &head->x_min));
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &head->y_min));
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &head->x_max));
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &head->y_max));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &head->mac_style));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &head->lowest_rec_ppem));
    PDF_PROPAGATE(sfnt_parser_read_int16(parser, &head->front_direction_hint));
    PDF_PROPAGATE(sfnt_parser_read_int16(parser, &head->idx_to_loc_format));
    PDF_PROPAGATE(sfnt_parser_read_int16(parser, &head->glyph_data_format));

    if (magic_number != 0x5f0f3cf5) {
        return PDF_ERR_SFNT_BAD_MAGIC;
    }

    return PDF_OK;
}
