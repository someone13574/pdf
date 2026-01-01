#include "maxp.h"

#include "err/error.h"
#include "logger/log.h"
#include "parser.h"

Error* sfnt_parse_maxp(SfntParser* parser, SfntMaxp* maxp) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(maxp);

    TRY(sfnt_parser_read_fixed(parser, &maxp->version));
    TRY(sfnt_parser_read_uint16(parser, &maxp->num_glyphs));

    if (maxp->version == 0x5000) {
        return NULL;
    } else if (maxp->version != 0x10000) {
        return ERROR(
            SFNT_ERR_INVALID_VERSION,
            "Invalid maxp version %u",
            maxp->version
        );
    }

    TRY(sfnt_parser_read_uint16(parser, &maxp->max_points));
    TRY(sfnt_parser_read_uint16(parser, &maxp->max_contours));
    TRY(sfnt_parser_read_uint16(parser, &maxp->max_component_points));
    TRY(sfnt_parser_read_uint16(parser, &maxp->max_component_contours));
    TRY(sfnt_parser_read_uint16(parser, &maxp->max_zones));
    TRY(sfnt_parser_read_uint16(parser, &maxp->max_twilight_points));
    TRY(sfnt_parser_read_uint16(parser, &maxp->max_storage));
    TRY(sfnt_parser_read_uint16(parser, &maxp->max_function_defs));
    TRY(sfnt_parser_read_uint16(parser, &maxp->max_instruction_defs));
    TRY(sfnt_parser_read_uint16(parser, &maxp->max_stack_elements));
    TRY(sfnt_parser_read_uint16(parser, &maxp->max_size_of_instructions));
    TRY(sfnt_parser_read_uint16(parser, &maxp->max_component_elements));
    TRY(sfnt_parser_read_uint16(parser, &maxp->max_component_depth));

    return NULL;
}
