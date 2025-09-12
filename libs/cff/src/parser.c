#include "parser.h"

#include <math.h>
#include <stdint.h>

#include "logger/log.h"
#include "pdf_error/error.h"
#include "types.h"

CffParser cff_parser_new(const uint8_t* buffer, size_t buffer_len) {
    RELEASE_ASSERT(buffer);

    return (CffParser
    ) {.buffer = buffer, .buffer_len = buffer_len, .offset = 0};
}

PdfError* cff_parser_seek(CffParser* parser, size_t offset) {
    RELEASE_ASSERT(parser);

    if (offset >= parser->buffer_len) {
        return PDF_ERROR(PDF_ERR_CFF_EOF);
    }

    parser->offset = offset;
    return NULL;
}

PdfError* cff_parser_read_card8(CffParser* parser, CffCard8* card8_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(card8_out);

    if (parser->offset >= parser->buffer_len) {
        return PDF_ERROR(PDF_ERR_CFF_EOF);
    }

    *card8_out = parser->buffer[parser->offset++];

    return NULL;
}

PdfError* cff_parser_read_card16(CffParser* parser, CffCard16* card16_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(card16_out);

    if (parser->offset + 1 >= parser->buffer_len) {
        return PDF_ERROR(PDF_ERR_CFF_EOF);
    }

    *card16_out = (CffCard16)((CffCard16)parser->buffer[parser->offset++] << 8);
    *card16_out |= (CffCard16)parser->buffer[parser->offset++];

    return NULL;
}

PdfError* cff_parser_read_offset(
    CffParser* parser,
    CffOffsetSize offset_size,
    CffOffset* offset_out
) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(offset_size >= 1 && offset_size <= 4);
    RELEASE_ASSERT(offset_out);

    if (parser->offset + offset_size - 1 >= parser->buffer_len) {
        return PDF_ERROR(PDF_ERR_CFF_EOF);
    }

    *offset_out = 0;
    for (size_t idx = 0; idx < offset_size; idx++) {
        *offset_out <<= 8;
        *offset_out |= (CffOffset)parser->buffer[parser->offset++];
    }

    return NULL;
}

PdfError*
cff_parser_read_offset_size(CffParser* parser, CffOffsetSize* offset_size_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(offset_size_out);

    if (parser->offset >= parser->buffer_len) {
        return PDF_ERROR(PDF_ERR_CFF_EOF);
    }

    *offset_size_out = parser->buffer[parser->offset++];
    if (*offset_size_out < 1 || *offset_size_out > 4) {
        return PDF_ERROR(
            PDF_ERR_CFF_INVALID_OFFSET_SIZE,
            "Offset size must be in range 1-4"
        );
    }

    return NULL;
}

PdfError* cff_parser_read_sid(CffParser* parser, CffStringID* sid_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(sid_out);

    CffCard16 card16;
    PDF_PROPAGATE(cff_parser_read_card16(parser, &card16));

    *sid_out = card16;
    if (card16 > 64999) {
        return PDF_ERROR(
            PDF_ERR_CFF_INVALID_SID,
            "CFF SID's must be in the range 0-64999"
        );
    }

    return NULL;
}

static PdfError*
read_int_operand(CffParser* parser, CffCard8 byte0, CffToken* token_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(token_out);

    token_out->type = CFF_TOKEN_INT_OPERAND;

    if (byte0 >= 32 && byte0 <= 246) {
        token_out->value.integer = (int32_t)byte0 - 139;
        return NULL;
    }

    CffCard8 byte1;
    PDF_PROPAGATE(cff_parser_read_card8(parser, &byte1));
    if (byte0 >= 247 && byte0 <= 250) {
        token_out->value.integer =
            ((int32_t)byte0 - 247) * 256 + (int32_t)byte1 + 108;
        return NULL;
    } else if (byte0 >= 251 && byte0 <= 254) {
        token_out->value.integer =
            -((int32_t)byte0 - 251) * 256 - (int32_t)byte1 - 108;
        return NULL;
    }

    CffCard8 byte2;
    PDF_PROPAGATE(cff_parser_read_card8(parser, &byte2));
    if (byte0 == 28) {
        uint16_t value = (uint16_t)((uint16_t)byte1 << 8) | (uint16_t)byte2;
        int16_t signed_value =
            *(int16_t*)&value; // Doesn't violate strict aliasing
                               // since corresponding signed/unsigned
                               // types can be aliased.

        token_out->value.integer = (int32_t)signed_value;
        return NULL;
    }

    if (byte0 != 29) {
        LOG_PANIC(
            "Unreachable. Byte 0 of an integer operand must be 28, 29, or 32-254 inclusive."
        );
    }

    CffCard8 byte3;
    CffCard8 byte4;
    PDF_PROPAGATE(cff_parser_read_card8(parser, &byte3));
    PDF_PROPAGATE(cff_parser_read_card8(parser, &byte4));

    uint32_t value = (uint32_t)byte1 << 24 | (uint32_t)byte2 << 16
                   | (uint32_t)byte3 | (uint32_t)byte4;
    token_out->value.integer =
        *(int32_t*)&value; // Doesn't violate strict aliasing. See above.

    return NULL;
}

static PdfError* read_real_operand(CffParser* parser, CffToken* token_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(token_out);

    token_out->type = CFF_TOKEN_REAL_OPERAND;
    token_out->value.real = 0.0;

    CffCard8 current_byte = 0;
    bool high_nibble = true;
    size_t processed_nibbles = 0;

    bool integer_part =
        true; // Use to track whether we are before the decimal point
    bool exp_part =
        false; // Use to track whether we are processing the exponential

    bool neg = false;
    bool exp_neg = false;
    int32_t exp_val = 0;
    double fractional_part_weight = 0.1;

    while (true) {
        CffCard8 nibble = 0;
        if (high_nibble) {
            PDF_PROPAGATE(cff_parser_read_card8(parser, &current_byte));
            nibble = (current_byte >> 4) & 0xf;
            high_nibble = false;
        } else {
            nibble = current_byte & 0xf;
            high_nibble = true;
        }

        if (nibble <= 9) {
            if (integer_part) {
                token_out->value.real *= 10.0;
                token_out->value.real += (double)nibble;
            } else if (exp_part) {
                exp_val *= 10;
                exp_val += nibble;
            } else {
                token_out->value.real +=
                    (double)nibble * fractional_part_weight;
                fractional_part_weight *= 0.1;
            }
        } else if (nibble == 0xa) {
            if (!integer_part) {
                return PDF_ERROR(
                    PDF_ERR_CFF_INVALID_REAL_OPERAND,
                    "Real cannot have more than one decimal and the exponent cannot be fractional"
                );
            }
        } else if (nibble == 0xb) {
            if (exp_part) {
                return PDF_ERROR(
                    PDF_ERR_CFF_INVALID_REAL_OPERAND,
                    "Real cannot have more than one exp part"
                );
            }

            integer_part = false;
            exp_part = true;
        } else if (nibble == 0xc) {
            if (exp_part) {
                return PDF_ERROR(
                    PDF_ERR_CFF_INVALID_REAL_OPERAND,
                    "Real cannot have more than one exp part"
                );
            }

            integer_part = false;
            exp_part = true;
            exp_neg = true;
        } else if (nibble == 0xe) {
            if (processed_nibbles != 0) {
                return PDF_ERROR(
                    PDF_ERR_CFF_INVALID_REAL_OPERAND,
                    "Minus sign not at start of real"
                );
            }
            neg = true;
        } else if (nibble == 0xf) {
            break;
        } else {
            return PDF_ERROR(
                PDF_ERR_CFF_RESERVED,
                "Nibble 0xd is reserved in real operands"
            );
        }

        processed_nibbles++;
    }

    if (neg) {
        token_out->value.real *= -1.0;
    }

    if (exp_part) {
        if (exp_neg) {
            exp_val = -exp_val;
        }

        token_out->value.real *= pow(10.0, (double)exp_val);
    }

    return NULL;
}

PdfError* cff_parser_read_token(CffParser* parser, CffToken* token_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(token_out);

    // Operators and operands may be distinguished by inspection of their first
    // byte: 0–21 specify operators and 28, 29, 30, and 32–254 specify operands
    // (numbers). Byte values 22–27, 31, and 255 are reserved.
    CffCard8 byte0;
    PDF_PROPAGATE(cff_parser_read_card8(parser, &byte0));

    if (byte0 <= 21) {
        token_out->type = CFF_TOKEN_OPERATOR;
        token_out->value.operator= byte0;
    } else if (byte0 == 30) {
        PDF_PROPAGATE(read_real_operand(parser, token_out));
    } else if (byte0 <= 27 || byte0 == 31 || byte0 == 255) {
        return PDF_ERROR(
            PDF_ERR_CFF_RESERVED,
            "Byte value %d is reserved in tokens",
            (int)byte0
        );
    } else {
        PDF_PROPAGATE(read_int_operand(parser, byte0, token_out));
    }

    LOG_TODO();
}

#ifdef TEST

// TODO: test CFF parser
// cff_parser_new
//   - test fields set correctly
// cff_parser_seek
//   - valid seek
//   - invalid seek
//   - eof seek
// cff_parser_read_card8
//   - first and second bytes
//   - last byte
//   - eof byte
// cff_parser_read_card16
//   - first and second byte
//   - second and third byte
//   - second last and last byte
//   - last and eof byte
// cff_parser_read_offset
//   - test reading offsets of sizes 1, 2, 3, and 4
//   - test eof
// cff_parser_read_offset_size
//   - test reading valid numbers (1, 2, 3, 4)
//   - test reading 0, 5
// cff_parser_read_sid
//   - test in range num
//   - test 64999
//   - test err 65000
//   - test eof
// cff_parser_read_token
//   - test operators from 0-21 via loop
//   - test integers from spec (one test fn per format)
//   - test real valid from spec
//   - invalid reals (+fn for reserved)
//   - reserved values (one fn)

#endif // TEST
