#include "header.h"

#include "logger/log.h"
#include "parser.h"
#include "pdf_error/error.h"

PdfError* cff_read_header(CffParser* parser, CffHeader* cff_header_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(cff_header_out);

    PDF_PROPAGATE(cff_parser_seek(parser, 0));
    PDF_PROPAGATE(cff_parser_read_card8(parser, &cff_header_out->major));
    PDF_PROPAGATE(cff_parser_read_card8(parser, &cff_header_out->minor));
    PDF_PROPAGATE(cff_parser_read_card8(parser, &cff_header_out->header_size));
    PDF_PROPAGATE(
        cff_parser_read_offset_size(parser, &cff_header_out->offset_size)
    );

    if (cff_header_out->major > 1) {
        return PDF_ERROR(CFF_ERR_UNSUPPORTED_VERSION);
    }

    return NULL;
}
