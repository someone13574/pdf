#include "header.h"

#include "err/error.h"
#include "logger/log.h"
#include "parser.h"

Error* cff_read_header(CffParser* parser, CffHeader* cff_header_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(cff_header_out);

    TRY(cff_parser_seek(parser, 0));
    TRY(cff_parser_read_card8(parser, &cff_header_out->major));
    TRY(cff_parser_read_card8(parser, &cff_header_out->minor));
    TRY(cff_parser_read_card8(parser, &cff_header_out->header_size));
    TRY(cff_parser_read_offset_size(parser, &cff_header_out->offset_size));

    if (cff_header_out->major > 1) {
        return ERROR(CFF_ERR_UNSUPPORTED_VERSION);
    }

    return NULL;
}
