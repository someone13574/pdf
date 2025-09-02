#include "codec/deflate.h"

#include <stdbool.h>
#include <stdint.h>

#include "bitstream.h"
#include "logger/log.h"
#include "pdf_error/error.h"

typedef struct {
    bool final;
    enum {
        DEFLATE_BLOCK_COMPRESSION_NONE,
        DEFLATE_BLOCK_COMPRESSION_FIXED,
        DEFLATE_BLOCK_COMPRESSION_DYN
    } type;
} DeflateBlockHeader;

static PdfError*
deflate_parse_block_header(BitStream* bitstream, DeflateBlockHeader* header) {
    RELEASE_ASSERT(bitstream);
    RELEASE_ASSERT(header);

    uint32_t read;
    PDF_PROPAGATE(bitstream_next(bitstream, &read));
    header->final = read == 1;

    PDF_PROPAGATE(bitstream_read_n(bitstream, 2, &read));
    switch (read) {
        case 0: {
            header->type = DEFLATE_BLOCK_COMPRESSION_NONE;
            break;
        }
        case 1: {
            header->type = DEFLATE_BLOCK_COMPRESSION_FIXED;
            break;
        }
        case 2: {
            header->type = DEFLATE_BLOCK_COMPRESSION_DYN;
            break;
        }
        default: {
            return PDF_ERROR(PDF_ERR_DEFLATE_INVALID_BLOCK_TYPE);
        }
    }

    return NULL;
}

PdfError* decode_deflate_data(uint8_t* data, size_t data_len) {
    RELEASE_ASSERT(data);

    BitStream bitstream = bitstream_new(data, data_len);

    DeflateBlockHeader header;
    PDF_PROPAGATE(deflate_parse_block_header(&bitstream, &header));

    return NULL;
}
