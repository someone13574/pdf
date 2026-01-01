#include "parser.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "pdf_error/error.h"
#include "types.h"

CffParser cff_parser_new(const uint8_t* buffer, size_t buffer_len) {
    RELEASE_ASSERT(buffer);

    return (CffParser) {.buffer = buffer,
                        .buffer_len = buffer_len,
                        .offset = 0};
}

PdfError* cff_parser_seek(CffParser* parser, size_t offset) {
    RELEASE_ASSERT(parser);

    if (offset > parser->buffer_len) {
        return PDF_ERROR(CFF_ERR_EOF);
    }

    parser->offset = offset;
    return NULL;
}

PdfError* cff_parser_read_card8(CffParser* parser, CffCard8* card8_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(card8_out);

    if (parser->offset >= parser->buffer_len) {
        return PDF_ERROR(CFF_ERR_EOF);
    }

    *card8_out = parser->buffer[parser->offset++];

    return NULL;
}

PdfError* cff_parser_read_card16(CffParser* parser, CffCard16* card16_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(card16_out);

    if (parser->offset + 1 >= parser->buffer_len) {
        return PDF_ERROR(CFF_ERR_EOF);
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
        return PDF_ERROR(CFF_ERR_EOF);
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
        return PDF_ERROR(CFF_ERR_EOF);
    }

    *offset_size_out = parser->buffer[parser->offset++];
    if (*offset_size_out < 1 || *offset_size_out > 4) {
        return PDF_ERROR(
            CFF_ERR_INVALID_OFFSET_SIZE,
            "Offset size must be in range 1-4"
        );
    }

    return NULL;
}

PdfError* cff_parser_read_sid(CffParser* parser, CffSID* sid_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(sid_out);

    CffCard16 card16;
    PDF_PROPAGATE(cff_parser_read_card16(parser, &card16));

    *sid_out = card16;
    if (card16 > 64999) {
        return PDF_ERROR(
            CFF_ERR_INVALID_SID,
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

    uint32_t value = ((uint32_t)byte1 << 24) | ((uint32_t)byte2 << 16)
                   | ((uint32_t)byte3 << 8) | (uint32_t)byte4;
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
                    CFF_ERR_INVALID_REAL_OPERAND,
                    "Real cannot have more than one decimal and the exponent cannot be fractional"
                );
            }

            integer_part = false;
        } else if (nibble == 0xb) {
            if (exp_part) {
                return PDF_ERROR(
                    CFF_ERR_INVALID_REAL_OPERAND,
                    "Real cannot have more than one exp part"
                );
            }

            integer_part = false;
            exp_part = true;
        } else if (nibble == 0xc) {
            if (exp_part) {
                return PDF_ERROR(
                    CFF_ERR_INVALID_REAL_OPERAND,
                    "Real cannot have more than one exp part"
                );
            }

            integer_part = false;
            exp_part = true;
            exp_neg = true;
        } else if (nibble == 0xe) {
            if (processed_nibbles != 0) {
                return PDF_ERROR(
                    CFF_ERR_INVALID_REAL_OPERAND,
                    "Minus sign not at start of real"
                );
            }
            neg = true;
        } else if (nibble == 0xf) {
            break;
        } else {
            return PDF_ERROR(
                CFF_ERR_RESERVED,
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
        token_out->value.operator = byte0;
    } else if (byte0 == 30) {
        PDF_PROPAGATE(read_real_operand(parser, token_out));
    } else if (byte0 <= 27 || byte0 == 31 || byte0 == 255) {
        return PDF_ERROR(
            CFF_ERR_RESERVED,
            "Byte value %d is reserved in tokens",
            (int)byte0
        );
    } else {
        PDF_PROPAGATE(read_int_operand(parser, byte0, token_out));
    }

    return NULL;
}

PdfError* cff_parser_get_str(
    Arena* arena,
    CffParser* parser,
    size_t length,
    char** str_out
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(str_out);

    if (parser->offset + length >= parser->buffer_len) {
        return PDF_ERROR(CFF_ERR_EOF);
    }

    *str_out = arena_alloc(arena, sizeof(char) * (length + 1));
    memcpy(*str_out, parser->buffer + parser->offset, length);
    (*str_out)[length] = '\0';

    return NULL;
}

#ifdef TEST

#include "test/test.h"

TEST_FUNC(test_cff_parser_new) {
    uint8_t buffer[] = "abc";
    CffParser parser =
        cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t) - 1);

    TEST_ASSERT(parser.buffer);
    TEST_ASSERT_EQ((size_t)3, parser.buffer_len);
    TEST_ASSERT_EQ((size_t)0, parser.offset);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_seek) {
    uint8_t buffer[] = "abc";
    CffParser parser =
        cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t) - 1);

    TEST_PDF_REQUIRE(cff_parser_seek(&parser, 1));
    TEST_ASSERT_EQ((size_t)1, parser.offset);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_seek_last) {
    uint8_t buffer[] = "abc";

    CffParser parser =
        cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t) - 1);

    TEST_PDF_REQUIRE(cff_parser_seek(&parser, 3));
    TEST_ASSERT_EQ((size_t)3, parser.offset);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_seek_invalid) {
    uint8_t buffer[] = "abc";

    CffParser parser =
        cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_PDF_REQUIRE_ERR(cff_parser_seek(&parser, 4), CFF_ERR_EOF);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_card8) {
    uint8_t buffer[] = "abc";

    CffParser parser =
        cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t) - 1);

    CffCard8 card8;
    TEST_PDF_REQUIRE(cff_parser_read_card8(&parser, &card8));
    TEST_ASSERT_EQ((CffCard8)'a', card8);
    TEST_PDF_REQUIRE(cff_parser_read_card8(&parser, &card8));
    TEST_ASSERT_EQ((CffCard8)'b', card8);
    TEST_PDF_REQUIRE(cff_parser_read_card8(&parser, &card8));
    TEST_ASSERT_EQ((CffCard8)'c', card8);
    TEST_PDF_REQUIRE_ERR(cff_parser_read_card8(&parser, &card8), CFF_ERR_EOF);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_card16_even) {
    uint8_t buffer[] = "abcdef";

    CffParser parser =
        cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t) - 1);

    CffCard16 card16;
    TEST_PDF_REQUIRE(cff_parser_read_card16(&parser, &card16));
    TEST_ASSERT_EQ((CffCard16)0x6162, card16);
    TEST_PDF_REQUIRE(cff_parser_read_card16(&parser, &card16));
    TEST_ASSERT_EQ((CffCard16)0x6364, card16);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_card16_odd) {
    uint8_t buffer[] = "abcdef";

    CffParser parser =
        cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_PDF_REQUIRE(cff_parser_seek(&parser, 1));

    CffCard16 card16;
    TEST_PDF_REQUIRE(cff_parser_read_card16(&parser, &card16));
    TEST_ASSERT_EQ((CffCard16)0x6263, card16);
    TEST_PDF_REQUIRE(cff_parser_read_card16(&parser, &card16));
    TEST_ASSERT_EQ((CffCard16)0x6465, card16);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_card16_last) {
    uint8_t buffer[] = "abcdef";

    CffParser parser =
        cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_PDF_REQUIRE(cff_parser_seek(&parser, 4));

    CffCard16 card16;
    TEST_PDF_REQUIRE(cff_parser_read_card16(&parser, &card16));
    TEST_ASSERT_EQ((CffCard16)0x6566, card16);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_card16_eof) {
    uint8_t buffer[] = "abcdef";
    CffParser parser =
        cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_PDF_REQUIRE(cff_parser_seek(&parser, 5));

    CffCard16 card16;
    TEST_PDF_REQUIRE_ERR(cff_parser_read_card16(&parser, &card16), CFF_ERR_EOF);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_offset) {
    uint8_t buffer[] = {0xa2, 0x2f, 0xe6, 0xf6, 0x42};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffOffset offset;
    TEST_PDF_REQUIRE(cff_parser_seek(&parser, 2));
    TEST_PDF_REQUIRE(cff_parser_read_offset(&parser, 1, &offset));
    TEST_ASSERT_EQ((CffOffset)0xe6, offset);

    TEST_PDF_REQUIRE(cff_parser_read_offset(&parser, 2, &offset));
    TEST_ASSERT_EQ((CffOffset)0xf642, offset);

    TEST_PDF_REQUIRE(cff_parser_seek(&parser, 1));
    TEST_PDF_REQUIRE(cff_parser_read_offset(&parser, 3, &offset));
    TEST_ASSERT_EQ((CffOffset)0x2fe6f6, offset);

    TEST_PDF_REQUIRE(cff_parser_seek(&parser, 0));
    TEST_PDF_REQUIRE(cff_parser_read_offset(&parser, 4, &offset));
    TEST_ASSERT_EQ((CffOffset)0xa22fe6f6, offset);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_offset_eof) {
    uint8_t buffer[] = {0xa2, 0x2f, 0xe6, 0xf6, 0x42};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffOffset offset;
    TEST_PDF_REQUIRE(cff_parser_seek(&parser, 2));
    TEST_PDF_REQUIRE_ERR(
        cff_parser_read_offset(&parser, 4, &offset),
        CFF_ERR_EOF
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_offset_size) {
    uint8_t buffer[] = {0x1, 0x2, 0x3, 0x4};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffOffsetSize offset_size;
    TEST_PDF_REQUIRE(cff_parser_read_offset_size(&parser, &offset_size));
    TEST_ASSERT_EQ((CffOffsetSize)1, offset_size);
    TEST_PDF_REQUIRE(cff_parser_read_offset_size(&parser, &offset_size));
    TEST_ASSERT_EQ((CffOffsetSize)2, offset_size);
    TEST_PDF_REQUIRE(cff_parser_read_offset_size(&parser, &offset_size));
    TEST_ASSERT_EQ((CffOffsetSize)3, offset_size);
    TEST_PDF_REQUIRE(cff_parser_read_offset_size(&parser, &offset_size));
    TEST_ASSERT_EQ((CffOffsetSize)4, offset_size);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_offset_size0) {
    uint8_t buffer[] = {0x0};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffOffsetSize offset_size;
    TEST_PDF_REQUIRE_ERR(
        cff_parser_read_offset_size(&parser, &offset_size),
        CFF_ERR_INVALID_OFFSET_SIZE
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_offset_size5) {
    uint8_t buffer[] = {0x5};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffOffsetSize offset_size;
    TEST_PDF_REQUIRE_ERR(
        cff_parser_read_offset_size(&parser, &offset_size),
        CFF_ERR_INVALID_OFFSET_SIZE
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_sid) {
    uint8_t buffer[] = {0x95, 0x5c, 0xd5, 0xc3};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffSID sid;
    TEST_PDF_REQUIRE(cff_parser_read_sid(&parser, &sid));
    TEST_ASSERT_EQ((CffSID)0x955c, sid);
    TEST_PDF_REQUIRE(cff_parser_read_sid(&parser, &sid));
    TEST_ASSERT_EQ((CffSID)0xd5c3, sid);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_sid_64999) {
    uint8_t buffer[] = {0xfd, 0xe7};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffSID sid;
    TEST_PDF_REQUIRE(cff_parser_read_sid(&parser, &sid));
    TEST_ASSERT_EQ((CffSID)64999, sid);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_sid_invalid_65000) {
    uint8_t buffer[] = {0xfd, 0xe8};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffSID sid;
    TEST_PDF_REQUIRE_ERR(
        cff_parser_read_sid(&parser, &sid),
        CFF_ERR_INVALID_SID
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_sid_eof) {
    uint8_t buffer[] = {0xfd};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffSID sid;
    TEST_PDF_REQUIRE_ERR(cff_parser_read_sid(&parser, &sid), CFF_ERR_EOF);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_operator) {
    for (uint8_t operator = 0; operator <= 21; operator++) {
        uint8_t buffer[] = {operator};
        CffParser parser =
            cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

        CffToken token;
        TEST_PDF_REQUIRE(cff_parser_read_token(&parser, &token));
        TEST_ASSERT_EQ((CffTokenType)CFF_TOKEN_OPERATOR, token.type);
        TEST_ASSERT_EQ(operator, token.value.operator);
    }

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_token_eof) {
    uint8_t buffer[] = {0};
    CffParser parser = cff_parser_new(buffer, 0);

    CffToken token;
    TEST_PDF_REQUIRE_ERR(cff_parser_read_token(&parser, &token), CFF_ERR_EOF);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_token_reserved) {
    uint8_t reserved_bytes[] = {22, 23, 24, 25, 26, 27, 31, 255};
    for (size_t idx = 0; idx < sizeof(reserved_bytes) / sizeof(uint8_t);
         idx++) {
        uint8_t buffer[] = {reserved_bytes[idx]};
        CffParser parser =
            cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

        CffToken token;
        TEST_PDF_REQUIRE_ERR(
            cff_parser_read_token(&parser, &token),
            CFF_ERR_RESERVED
        );
    }

    return TEST_RESULT_PASS;
}

// From 'Table 4 Integer Format Examples' of
// https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf.
TEST_FUNC(test_cff_parser_read_int_operand) {
    uint8_t buffer[] = {0x8b, 0xef, 0x27, 0xfa, 0x7c, 0xfe, 0x7c, 0x1c,
                        0x27, 0x10, 0x1c, 0xd8, 0xf0, 0x1d, 0x00, 0x01,
                        0x86, 0xa0, 0x1d, 0xff, 0xfe, 0x79, 0x60};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffToken token;
    int32_t expected_values[] =
        {0, 100, -100, 1000, -1000, 10000, -10000, 100000, -100000};
    for (size_t idx = 0; idx < sizeof(expected_values) / sizeof(int32_t);
         idx++) {
        TEST_PDF_REQUIRE(cff_parser_read_token(&parser, &token));
        TEST_ASSERT_EQ((CffTokenType)CFF_TOKEN_INT_OPERAND, token.type);
        TEST_ASSERT_EQ(expected_values[idx], token.value.integer);
    }

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_real_operand) {
    uint8_t buffer[] =
        {0x1e, 0xe2, 0xa2, 0x5f, 0x1e, 0x0a, 0x14, 0x05, 0x41, 0xc3, 0xff};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffToken token;
    TEST_PDF_REQUIRE(cff_parser_read_token(&parser, &token));
    TEST_ASSERT_EQ((CffTokenType)CFF_TOKEN_REAL_OPERAND, token.type);
    TEST_ASSERT_EQ(-2.25, token.value.real);

    TEST_PDF_REQUIRE(cff_parser_read_token(&parser, &token));
    TEST_ASSERT_EQ((CffTokenType)CFF_TOKEN_REAL_OPERAND, token.type);
    TEST_ASSERT_EQ_EPS(0.140541e-3, token.value.real, 1e-9L);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_real_operand_no_fractional) {
    uint8_t buffer[] = {0x1e, 0x5f};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffToken token;
    TEST_PDF_REQUIRE(cff_parser_read_token(&parser, &token));
    TEST_ASSERT_EQ((CffTokenType)CFF_TOKEN_REAL_OPERAND, token.type);
    TEST_ASSERT_EQ(5.0, token.value.real);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_real_operand_no_fractional_with_exp) {
    uint8_t buffer[] = {0x1e, 0x5b, 0x3f};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffToken token;
    TEST_PDF_REQUIRE(cff_parser_read_token(&parser, &token));
    TEST_ASSERT_EQ((CffTokenType)CFF_TOKEN_REAL_OPERAND, token.type);
    TEST_ASSERT_EQ(5e3, token.value.real);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_real_operand_no_integer) {
    uint8_t buffer[] = {0x1e, 0xa5, 0xff};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffToken token;
    TEST_PDF_REQUIRE(cff_parser_read_token(&parser, &token));
    TEST_ASSERT_EQ((CffTokenType)CFF_TOKEN_REAL_OPERAND, token.type);
    TEST_ASSERT_EQ(0.5, token.value.real);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_real_operand_fractional_exp_err) {
    uint8_t buffer[] = {0x1e, 0x5b, 0x2a, 0x5f};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffToken token;
    TEST_PDF_REQUIRE_ERR(
        cff_parser_read_token(&parser, &token),
        CFF_ERR_INVALID_REAL_OPERAND
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_real_operand_trailing_zeros) {
    uint8_t buffer[] = {0x1e, 0x5a, 0x50, 0x00, 0x0f};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffToken token;
    TEST_PDF_REQUIRE(cff_parser_read_token(&parser, &token));
    TEST_ASSERT_EQ((CffTokenType)CFF_TOKEN_REAL_OPERAND, token.type);
    TEST_ASSERT_EQ(5.5, token.value.real);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parser_read_real_operand_reserved_nibble) {
    uint8_t buffer[] = {0x1e, 0x5a, 0x50, 0xd0, 0x0f};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffToken token;
    TEST_PDF_REQUIRE_ERR(
        cff_parser_read_token(&parser, &token),
        CFF_ERR_RESERVED
    );

    return TEST_RESULT_PASS;
}

#endif // TEST
