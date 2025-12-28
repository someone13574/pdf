#include "maxp.h"

#include "logger/log.h"
#include "parser.h"
#include "pdf_error/error.h"

PdfError* sfnt_parse_maxp(SfntParser* parser, SfntMaxp* maxp) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(maxp);

    PDF_PROPAGATE(sfnt_parser_read_fixed(parser, &maxp->version));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &maxp->num_glyphs));

    if (maxp->version == 0x5000) {
        return NULL;
    } else if (maxp->version != 0x10000) {
        return PDF_ERROR(
            PDF_ERR_SFNT_INVALID_VERSION,
            "Invalid maxp version %u",
            maxp->version
        );
    }

    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &maxp->max_points));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &maxp->max_contours));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &maxp->max_component_points));
    PDF_PROPAGATE(
        sfnt_parser_read_uint16(parser, &maxp->max_component_contours)
    );
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &maxp->max_zones));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &maxp->max_twilight_points));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &maxp->max_storage));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &maxp->max_function_defs));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &maxp->max_instruction_defs));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &maxp->max_stack_elements));
    PDF_PROPAGATE(
        sfnt_parser_read_uint16(parser, &maxp->max_size_of_instructions)
    );
    PDF_PROPAGATE(
        sfnt_parser_read_uint16(parser, &maxp->max_component_elements)
    );
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &maxp->max_component_depth));

    return NULL;
}
