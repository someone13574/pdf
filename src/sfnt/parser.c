
#include "parser.h"

#include <stdint.h>

#include "log.h"
#include "pdf/result.h"

void sfnt_parser_new(uint8_t* buffer, size_t buffer_len, SfntParser* parser) {
    RELEASE_ASSERT(buffer);
    RELEASE_ASSERT(parser);

    parser->buffer = buffer;
    parser->buffer_len = buffer_len;
    parser->offset = 0;
}

PdfResult sfnt_subparser_new(
    SfntParser* parent,
    uint32_t offset,
    uint32_t len,
    uint32_t checksum,
    SfntParser* parser
) {
    RELEASE_ASSERT(parent);
    RELEASE_ASSERT(parser);

    if (offset + len > parent->buffer_len) {
        return PDF_ERR_SFNT_EOF;
    }

    parser->buffer = parent->buffer + offset;
    parser->buffer_len = len;
    parser->offset = 0;

    // Check checksum
    uint32_t num_words = (len + 3) / 4;
    uint32_t sum = 0;
    for (uint32_t word_offset = 0; word_offset < num_words * 4;
         word_offset += 4) {
        if (word_offset + 4 <= len) {
            sum += ((uint32_t)parent->buffer[offset + word_offset] << 24)
                | ((uint32_t)parent->buffer[offset + word_offset + 1] << 16)
                | ((uint32_t)parent->buffer[offset + word_offset + 2] << 8)
                | (uint32_t)parent->buffer[offset + word_offset + 3];
        } else {
            uint32_t word = 0;

            for (size_t byte_idx = 0; byte_idx + word_offset < len;
                 byte_idx++) {
                word |=
                    (uint32_t)parent->buffer[offset + word_offset + byte_idx]
                    << (24 - 8 * byte_idx);
            }

            sum += word;
        }
    }

    if (sum != checksum) {
        LOG_WARN_G(
            "sfnt",
            "SFNT checksum mismatch %llu != %llu",
            (unsigned long long)checksum,
            (unsigned long long)sum
        );
        return PDF_ERR_SFNT_TABLE_CHECKSUM;
    }

    return PDF_OK;
}

PdfResult sfnt_parser_read_uint16(SfntParser* parser, uint16_t* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    if (parser->offset + 2 > parser->buffer_len) {
        return PDF_ERR_SFNT_EOF;
    }

    uint32_t parsed = ((uint32_t)parser->buffer[parser->offset] << 8)
        | (uint32_t)parser->buffer[parser->offset + 1];
    *out = (uint16_t)parsed;
    parser->offset += 2;

    return PDF_OK;
}

PdfResult sfnt_parser_read_uint32(SfntParser* parser, uint32_t* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    if (parser->offset + 4 > parser->buffer_len) {
        return PDF_ERR_SFNT_EOF;
    }

    *out = ((uint32_t)parser->buffer[parser->offset] << 24)
        | ((uint32_t)parser->buffer[parser->offset + 1] << 16)
        | ((uint32_t)parser->buffer[parser->offset + 2] << 8)
        | (uint32_t)parser->buffer[parser->offset + 3];
    parser->offset += 4;

    return PDF_OK;
}

PdfResult
sfnt_parser_read_uint16_array(SfntParser* parser, SfntUint16Array* array) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(array);

    for (size_t idx = 0; idx < sfnt_uint16_array_len(array); idx++) {
        uint16_t word;
        PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &word));
        sfnt_uint16_array_set(array, idx, word);
    }

    return PDF_OK;
}
