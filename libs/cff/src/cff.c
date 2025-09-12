#include "cff/cff.h"

#include "header.h"
#include "logger/log.h"
#include "parser.h"
#include "pdf_error/error.h"

PdfError*
cff_parse(const uint8_t* data, size_t data_len, CffFont** cff_font_out) {
    RELEASE_ASSERT(data);
    RELEASE_ASSERT(cff_font_out);

    CffParser parser = cff_parser_new(data, data_len);

    CffHeader header;
    PDF_PROPAGATE(cff_read_header(&parser, &header));

    LOG_TODO();

    return NULL;
}
