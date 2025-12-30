
#include "parser.h"

#include <stdint.h>

#include "logger/log.h"
#include "pdf_error/error.h"
#include "sfnt/types.h"

void sfnt_parser_new(
    const uint8_t* buffer,
    size_t buffer_len,
    SfntParser* parser
) {
    RELEASE_ASSERT(buffer);
    RELEASE_ASSERT(parser);

    parser->buffer = buffer;
    parser->buffer_len = buffer_len;
    parser->offset = 0;
}

PdfError* sfnt_subparser_new(
    SfntParser* parent,
    uint32_t offset,
    uint32_t len,
    uint32_t checksum,
    bool head_table,
    SfntParser* parser
) {
    RELEASE_ASSERT(parent);
    RELEASE_ASSERT(parser);

    if (offset + len > parent->buffer_len) {
        return PDF_ERROR(
            PDF_ERR_SFNT_EOF,
            "Sfnt subparser length overflowed parent bounds"
        );
    }

    parser->buffer = parent->buffer + offset;
    parser->buffer_len = len;
    parser->offset = 0;

    // Check checksum
    uint32_t num_words = (len + 3) / 4;
    uint32_t sum = 0;
    for (uint32_t byte_offset = 0; byte_offset < num_words * 4;
         byte_offset += 4) {
        if (head_table && byte_offset == 8) {
            // Ignore checksum adjustment
            continue;
        }

        if (byte_offset + 4 <= len) {
            sum += ((uint32_t)parent->buffer[offset + byte_offset] << 24)
                 | ((uint32_t)parent->buffer[offset + byte_offset + 1] << 16)
                 | ((uint32_t)parent->buffer[offset + byte_offset + 2] << 8)
                 | (uint32_t)parent->buffer[offset + byte_offset + 3];
        } else {
            uint32_t word = 0;

            for (size_t byte_idx = 0; byte_idx + byte_offset < len;
                 byte_idx++) {
                word |=
                    (uint32_t)parent->buffer[offset + byte_offset + byte_idx]
                    << (24 - 8 * byte_idx);
            }

            sum += word;
        }
    }

    if (sum != checksum) {
        return PDF_ERROR(
            PDF_ERR_SFNT_TABLE_CHECKSUM,
            "SFNT checksum mismatch %llu != %llu",
            (unsigned long long)checksum,
            (unsigned long long)sum
        );
    }

    return NULL;
}

PdfError* sfnt_parser_seek(SfntParser* parser, size_t offset) {
    RELEASE_ASSERT(parser);

    if (offset >= parser->buffer_len) {
        return PDF_ERROR(PDF_ERR_SFNT_EOF);
    }

    parser->offset = offset;
    return NULL;
}

PdfError* sfnt_parser_read_int8(SfntParser* parser, int8_t* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    if (parser->offset + 1 > parser->buffer_len) {
        return PDF_ERROR(PDF_ERR_SFNT_EOF);
    }

    *out = (int8_t)parser->buffer[parser->offset++];
    return NULL;
}

PdfError* sfnt_parser_read_uint8(SfntParser* parser, uint8_t* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    if (parser->offset + 1 > parser->buffer_len) {
        return PDF_ERROR(PDF_ERR_SFNT_EOF);
    }

    *out = parser->buffer[parser->offset++];
    return NULL;
}

PdfError* sfnt_parser_read_int16(SfntParser* parser, int16_t* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    uint16_t x = 0;
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &x));
    *out = (int16_t)x;
    return NULL;
}

PdfError* sfnt_parser_read_uint16(SfntParser* parser, uint16_t* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    if (parser->offset + 2 > parser->buffer_len) {
        return PDF_ERROR(PDF_ERR_SFNT_EOF);
    }

    uint32_t parsed = ((uint32_t)parser->buffer[parser->offset] << 8)
                    | (uint32_t)parser->buffer[parser->offset + 1];
    *out = (uint16_t)parsed;
    parser->offset += 2;

    return NULL;
}

PdfError* sfnt_parser_read_int32(SfntParser* parser, int32_t* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    uint32_t x = 0;
    PDF_PROPAGATE(sfnt_parser_read_uint32(parser, &x));
    *out = (int32_t)x;
    return NULL;
}

PdfError* sfnt_parser_read_uint32(SfntParser* parser, uint32_t* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    if (parser->offset + 4 > parser->buffer_len) {
        return PDF_ERROR(PDF_ERR_SFNT_EOF);
    }

    *out = ((uint32_t)parser->buffer[parser->offset] << 24)
         | ((uint32_t)parser->buffer[parser->offset + 1] << 16)
         | ((uint32_t)parser->buffer[parser->offset + 2] << 8)
         | (uint32_t)parser->buffer[parser->offset + 3];
    parser->offset += 4;

    return NULL;
}

PdfError* sfnt_parser_read_int64(SfntParser* parser, int64_t* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    uint64_t x = 0;
    PDF_PROPAGATE(sfnt_parser_read_uint64(parser, &x));
    *out = (int64_t)x;
    return NULL;
}

PdfError* sfnt_parser_read_uint64(SfntParser* parser, uint64_t* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    if (parser->offset + 8 > parser->buffer_len) {
        return PDF_ERROR(PDF_ERR_SFNT_EOF);
    }

    *out = ((uint64_t)parser->buffer[parser->offset] << 56)
         | ((uint64_t)parser->buffer[parser->offset + 1] << 48)
         | ((uint64_t)parser->buffer[parser->offset + 2] << 40)
         | ((uint64_t)parser->buffer[parser->offset + 3] << 32)
         | ((uint64_t)parser->buffer[parser->offset + 4] << 24)
         | ((uint64_t)parser->buffer[parser->offset + 5] << 16)
         | ((uint64_t)parser->buffer[parser->offset + 6] << 8)
         | (uint64_t)parser->buffer[parser->offset + 7];
    parser->offset += 8;

    return NULL;
}

PdfError* sfnt_parser_read_uint8_array(SfntParser* parser, Uint8Array* array) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(array);

    for (size_t idx = 0; idx < uint8_array_len(array); idx++) {
        uint8_t byte = 0;
        PDF_PROPAGATE(sfnt_parser_read_uint8(parser, &byte));
        uint8_array_set(array, idx, byte);
    }

    return NULL;
}

PdfError*
sfnt_parser_read_uint16_array(SfntParser* parser, Uint16Array* array) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(array);

    for (size_t idx = 0; idx < uint16_array_len(array); idx++) {
        uint16_t word = 0;
        PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &word));
        uint16_array_set(array, idx, word);
    }

    return NULL;
}

PdfError* sfnt_parser_read_short_frac(SfntParser* parser, SfntShortFrac* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    int16_t x = 0;
    PDF_PROPAGATE(sfnt_parser_read_int16(parser, &x));
    *out = (SfntShortFrac)x;
    return NULL;
}

PdfError* sfnt_parser_read_fixed(SfntParser* parser, SfntFixed* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    int32_t x = 0;
    PDF_PROPAGATE(sfnt_parser_read_int32(parser, &x));
    *out = (SfntFixed)x;
    return NULL;
}

PdfError* sfnt_parser_read_fword(SfntParser* parser, SfntFWord* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    int16_t x = 0;
    PDF_PROPAGATE(sfnt_parser_read_int16(parser, &x));
    *out = (SfntFWord)x;
    return NULL;
}

PdfError* sfnt_parser_read_ufword(SfntParser* parser, SfntUFWord* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    uint16_t x = 0;
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &x));
    *out = (SfntUFWord)x;
    return NULL;
}

PdfError*
sfnt_parser_read_long_date_time(SfntParser* parser, SfntLongDateTime* out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(out);

    int64_t x = 0;
    PDF_PROPAGATE(sfnt_parser_read_int64(parser, &x));
    *out = (SfntLongDateTime)x;
    return NULL;
}
