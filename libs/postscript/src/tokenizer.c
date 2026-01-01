#include "postscript/tokenizer.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "arena/arena.h"
#include "err/error.h"
#include "logger/log.h"

struct PSTokenizer {
    Arena* arena;

    const uint8_t* data;
    size_t data_len;
    size_t offset;
};

PSTokenizer*
ps_tokenizer_new(Arena* arena, const uint8_t* data, size_t data_len) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(data);

    PSTokenizer* tokenizer = arena_alloc(arena, sizeof(PSTokenizer));
    tokenizer->arena = arena;
    tokenizer->data = data;
    tokenizer->data_len = data_len;
    tokenizer->offset = 0;

    return tokenizer;
}

static bool is_postscript_whitespace(uint8_t c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\0'
        || c == '\f';
}

static bool is_postscript_delimiter(uint8_t c) {
    return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']'
        || c == '{' || c == '}' || c == '/' || c == '%';
}

static bool is_postscript_regular(uint8_t c) {
    return !is_postscript_whitespace(c) && !is_postscript_delimiter(c);
}

static void consume_whitespace(PSTokenizer* tokenizer) {
    RELEASE_ASSERT(tokenizer);

    while (tokenizer->offset < tokenizer->data_len) {
        uint8_t c = tokenizer->data[tokenizer->offset];
        if (is_postscript_whitespace(c)) {
            tokenizer->offset++;
        } else {
            break;
        }
    }
}

static bool consume_comment(PSTokenizer* tokenizer) {
    RELEASE_ASSERT(tokenizer);

    consume_whitespace(tokenizer);

    if (tokenizer->offset >= tokenizer->data_len
        || tokenizer->data[tokenizer->offset] != '%') {
        return false;
    }

    while (tokenizer->offset < tokenizer->data_len) {
        uint8_t c = tokenizer->data[tokenizer->offset++];
        if (c == '\n') {
            break;
        } else if (c == '\r') {
            consume_whitespace(tokenizer);
            break;
        }
    }

    return true;
}

/// Reads the largest integer possible from the stream, starting at the current
/// offset. If the size exceeds the 32-bit unsigned limit, it will be output as
/// a real instead. This function doesn't check for a trailing delimiter.
static void read_unsigned_integer(
    PSTokenizer* tokenizer,
    uint8_t radix,
    size_t* read_length_out,
    uint64_t* integer_out,
    double* real_out,
    bool* is_integer_out
) {
    RELEASE_ASSERT(tokenizer);
    RELEASE_ASSERT(radix >= 2 && radix <= 36);
    RELEASE_ASSERT(read_length_out);
    RELEASE_ASSERT(integer_out);
    RELEASE_ASSERT(real_out);
    RELEASE_ASSERT(is_integer_out);

    *read_length_out = 0;
    *integer_out = 0;
    *real_out = 0;
    *is_integer_out = true;

    while (tokenizer->offset < tokenizer->data_len) {
        uint8_t c = tokenizer->data[tokenizer->offset];

        // Determine value of character
        uint64_t character_value;
        if (c >= '0' && c <= '9') {
            if (c >= '0' + radix) {
                break;
            }

            character_value = (uint64_t)(c - '0');
        } else if (c >= 'a' && c <= 'z') {
            if (c >= 'a' + radix - 10) {
                break;
            }

            character_value = (uint64_t)(c + 10 - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            if (c >= 'A' + radix - 10) {
                break;
            }

            character_value = (uint64_t)(c + 10 - 'A');
        } else {
            break;
        }

        *read_length_out += 1;
        tokenizer->offset++;

        // Shift integer
        if (*is_integer_out) {
            *integer_out *= radix;
            *integer_out += character_value;

            if (*integer_out > UINT32_MAX) {
                *real_out = (double)*integer_out;
                *is_integer_out = false;
            }

            continue;
        }

        // Shift real
        *real_out *= radix;
        *real_out += (double)character_value;
    }
}

static char* read_name(PSTokenizer* tokenizer, size_t known_length) {
    RELEASE_ASSERT(tokenizer);

    // Measure length
    while (tokenizer->offset < tokenizer->data_len) {
        uint8_t c = tokenizer->data[tokenizer->offset];

        if (is_postscript_regular(c)) {
            tokenizer->offset++;
            known_length++;
        } else {
            break;
        }
    }

    // Copy name
    RELEASE_ASSERT(known_length <= tokenizer->offset);

    char* name =
        arena_alloc(tokenizer->arena, sizeof(char) * (known_length + 1));
    memcpy(
        name,
        tokenizer->data + tokenizer->offset - known_length,
        sizeof(char) * known_length
    );
    name[known_length] = '\0';

    return name;
}

/// Attempts to real a number, and then falls back to treating it as a name. If
/// the leading character is a number, the offset must be set to the start of
/// the number, otherwise, it must exclude that character. `name_read_offset`
/// corresponds to the offset which would be used to read a name if it needs to
/// fall back to that.
static Error* read_number_or_executable_name(
    PSTokenizer* tokenizer,
    size_t name_read_offset,
    bool negative,
    bool has_leading_sign,
    bool has_leading_decimal,
    PSToken* token_out
) {
    RELEASE_ASSERT(tokenizer);
    RELEASE_ASSERT(token_out);

    // Handle unavailable buffer
    if (tokenizer->offset >= tokenizer->data_len) {
        if (has_leading_sign || has_leading_decimal) {
            token_out->type = PS_TOKEN_EXE_NAME;
            token_out->data.name =
                has_leading_sign ? (negative ? "-" : "+") : ".";
            return NULL;
        } else {
            // Impossible, since the caller should have adjusted the offset back
            // one to include the digit.
            LOG_PANIC("Unreachable");
        }
    }

    // Check for dot after leading sign
    if (has_leading_sign) {
        if (tokenizer->data[tokenizer->offset] == '.') {
            tokenizer->offset++;
            has_leading_decimal = true;
        }
    }

    // Read integer part
    size_t integer_part_read_digits = 0;
    uint64_t integer_part_int = 0;
    double integer_part_real = 0.0;
    bool is_integer_part_int = true;

    if (!has_leading_decimal) {
        read_unsigned_integer(
            tokenizer,
            10,
            &integer_part_read_digits,
            &integer_part_int,
            &integer_part_real,
            &is_integer_part_int
        );

        // The integer ends
        if (integer_part_read_digits != 0
            && (tokenizer->offset >= tokenizer->data_len
                || !is_postscript_regular(
                    tokenizer->data[tokenizer->offset]
                ))) {
            uint64_t max_val =
                negative ? (uint64_t)2147483648LL : (uint64_t)2147483647;
            if (is_integer_part_int && integer_part_int > max_val) {
                is_integer_part_int = false;
                integer_part_real = (double)integer_part_int;
            }

            if (is_integer_part_int) {
                token_out->type = PS_TOKEN_INTEGER;
                token_out->data.integer = negative ? (int32_t)-integer_part_int
                                                   : (int32_t)integer_part_int;
            } else {
                token_out->type = PS_TOKEN_REAL;
                token_out->data.real =
                    negative ? -integer_part_real : integer_part_real;
            }

            return NULL;
        }
    }

    // Try to read radix number
    if (!has_leading_sign && !has_leading_decimal
        && tokenizer->offset < tokenizer->data_len
        && tokenizer->data[tokenizer->offset] == '#' && is_integer_part_int
        && integer_part_int >= 2 && integer_part_int <= 36) {
        size_t radix_restore_offset = tokenizer->offset;
        tokenizer->offset++;

        size_t read_length;
        uint64_t integer_val;
        double real_val;
        bool is_int_val;

        read_unsigned_integer(
            tokenizer,
            (uint8_t)integer_part_int,
            &read_length,
            &integer_val,
            &real_val,
            &is_int_val
        );

        // If the number is unterminated, we want to treat it as a name instead
        // of as limitcheck.
        bool unterminated =
            tokenizer->offset < tokenizer->data_len
            && is_postscript_regular(tokenizer->data[tokenizer->offset]);

        if (read_length > 0 && !unterminated) {
            if (!is_int_val || integer_val > UINT32_MAX) {
                return ERROR(PS_ERR_LIMITCHECK, "Radix number too large");
            }

            union UintToSint {
                uint32_t uint;
                int32_t sint;
            } converter = {.uint = (uint32_t)integer_val};

            token_out->type = PS_TOKEN_RADIX_NUM;
            token_out->data.integer = converter.sint;
            return NULL;
        } else {
            tokenizer->offset = radix_restore_offset;
        }
    }

    // Try to read real part
    bool has_decimal = has_leading_decimal;
    if (!has_leading_decimal && tokenizer->data[tokenizer->offset] == '.') {
        tokenizer->offset++;
        has_decimal = true;
    }

    double real_val = integer_part_real;
    if (is_integer_part_int) {
        real_val = (double)integer_part_int;
    }

    size_t real_part_read_digits = 0;
    if (has_decimal) {
        uint64_t real_part_int;
        double real_part_real;
        bool is_real_part_int;

        read_unsigned_integer(
            tokenizer,
            10,
            &real_part_read_digits,
            &real_part_int,
            &real_part_real,
            &is_real_part_int
        );

        if (is_real_part_int) {
            real_part_real = (double)real_part_int;
        }

        if (real_part_read_digits != 0) {
            real_part_real /= pow(10.0, (double)real_part_read_digits);
        }

        real_val += real_part_real;
    }

    // All numbers *must* have a decimal digit in it, so this is a name
    if (integer_part_read_digits == 0 && real_part_read_digits == 0) {
        tokenizer->offset = name_read_offset;
        token_out->type = PS_TOKEN_EXE_NAME;
        token_out->data.name = read_name(tokenizer, 0);
        return NULL;
    }

    if (tokenizer->offset >= tokenizer->data_len) {
        token_out->type = PS_TOKEN_REAL;
        token_out->data.real = negative ? -real_val : real_val;
        return NULL;
    }

    // Try to read exponent
    uint8_t c = tokenizer->data[tokenizer->offset];
    bool failed_exp = false;
    if (c == 'e' || c == 'E') {
        tokenizer->offset++;

        bool exp_neg = false;
        if (tokenizer->offset < tokenizer->data_len) {
            uint8_t sign_char = tokenizer->data[tokenizer->offset];
            if (sign_char == '+') {
                tokenizer->offset++;
            } else if (sign_char == '-') {
                exp_neg = true;
                tokenizer->offset++;
            }
        }

        size_t read_digits;
        uint64_t exp_int;
        double exp_real;
        bool is_exp_int;

        read_unsigned_integer(
            tokenizer,
            10,
            &read_digits,
            &exp_int,
            &exp_real,
            &is_exp_int
        );

        if (is_exp_int && read_digits > 0) {
            real_val *= pow(10.0, exp_neg ? -(double)exp_int : (double)exp_int);
        } else {
            failed_exp = true;
        }
    }

    // Determine if it is a name or number
    if (!failed_exp
        && (tokenizer->offset >= tokenizer->data_len
            || !is_postscript_regular(tokenizer->data[tokenizer->offset]))) {
        token_out->type = PS_TOKEN_REAL;
        token_out->data.real = negative ? -real_val : real_val;
        return NULL;
    }

    // Treat as name
    tokenizer->offset = name_read_offset;
    token_out->type = PS_TOKEN_EXE_NAME;
    token_out->data.name = read_name(tokenizer, 0);
    return NULL;
}

static Error*
read_lit_string(PSTokenizer* tokenizer, PSString* string_out, bool first_pass) {
    RELEASE_ASSERT(tokenizer);
    RELEASE_ASSERT(string_out);

    size_t restore_offset = tokenizer->offset;

    if (first_pass) {
        string_out->data = NULL;
        string_out->len = 0;
    }

    int open_parenthesis = 1;
    int escape = 0;
    uint16_t octal_builder = 0; // used for constructing escaped octal values
    size_t write_idx = 0;

    while (tokenizer->offset < tokenizer->data_len) {
        uint8_t c = (uint8_t)tokenizer->data[tokenizer->offset++];

        if (escape > 0) {
            // Handle line-continuation escapes: LF, CR, or CRLF. Skips when
            // escaped
            if (c == '\n') {
                escape = 0;
                continue;
            } else if (c == '\r') {
                escape = 0;
                if (tokenizer->offset < tokenizer->data_len
                    && tokenizer->data[tokenizer->offset] == '\n') {
                    tokenizer->offset++;
                }
                continue;
            }

            if (c >= '0' && c <= '7') {
                RELEASE_ASSERT(escape <= 3);

                octal_builder |= (uint16_t)((c - '0') << (3 - escape) * 3);
                escape++;

                if (escape == 4) {
                    escape = 0;
                    c = (uint8_t)octal_builder;
                } else {
                    continue;
                }
            } else if (escape > 1) {
                // Emit accumulated octal, shifting to account for it not
                // be fully built.
                c = (uint8_t)(octal_builder >> (4 - escape) * 3);
                escape = 0;
                tokenizer
                    ->offset--; // the current character isn't being emitted,
                                // the octal one is, so we need to reprocess it
            } else {
                escape = 0;

                switch (c) {
                    case 'n': {
                        c = '\n';
                        break;
                    }
                    case 'r': {
                        c = '\r';
                        break;
                    }
                    case 't': {
                        c = '\t';
                        break;
                    }
                    case 'b': {
                        c = '\b';
                        break;
                    }
                    case 'f': {
                        c = '\f';
                        break;
                    }
                    case '\\': {
                        c = '\\';
                        break;
                    }
                    case '(': {
                        c = '(';
                        break;
                    }
                    case ')': {
                        c = ')';
                        break;
                    }
                    default: {
                        break;
                    }
                }
            }
        } else if (c == '(') {
            open_parenthesis++;
        } else if (c == ')') {
            open_parenthesis--;

            if (open_parenthesis == 0) {
                break;
            }
        } else if (c == '\\') {
            escape = 1;
            octal_builder = 0;
            continue;
        }

        if (first_pass) {
            string_out->len++;
        } else {
            RELEASE_ASSERT(write_idx < string_out->len);
            string_out->data[write_idx++] = c;
        }
    }

    if (open_parenthesis != 0) {
        return ERROR(PS_ERR_EOF);
    }

    if (first_pass) {
        string_out->data =
            arena_alloc(tokenizer->arena, sizeof(uint8_t) * string_out->len);
        tokenizer->offset = restore_offset;
        return read_lit_string(tokenizer, string_out, false);
    } else {
        return NULL;
    }
}

static bool char_to_hex(uint8_t c, int* out) {
    if (c >= '0' && c <= '9') {
        *out = c - '0';
        return true;
    } else if (c >= 'A' && c <= 'F') {
        *out = c - 'A' + 10;
        return true;
    } else if (c >= 'a' && c <= 'f') {
        *out = c - 'a' + 10;
        return true;
    } else {
        return false;
    }
}

static Error*
read_hex_string(PSTokenizer* tokenizer, PSString* string_out, bool first_pass) {
    RELEASE_ASSERT(tokenizer);
    RELEASE_ASSERT(string_out);

    size_t restore_offset = tokenizer->offset;

    if (first_pass) {
        string_out->data = NULL;
        string_out->len = 0;
    }

    size_t write_offset = 0;
    bool upper = true;

    while (tokenizer->offset < tokenizer->data_len) {
        uint8_t c = tokenizer->data[tokenizer->offset++];

        if (is_postscript_whitespace(c)) {
            continue;
        }

        if (c == '>') {
            break;
        }

        int hex;
        if (char_to_hex(c, &hex)) {
            if (upper) {
                if (first_pass) {
                    string_out->len++;
                } else {
                    string_out->data[write_offset] = (uint8_t)(hex << 4);
                }

                upper = false;
            } else {
                if (!first_pass) {
                    string_out->data[write_offset] |= (uint8_t)hex;
                }

                upper = true;
                write_offset++;
            }
        } else {
            return ERROR(
                PS_ERR_INVALID_CHAR,
                "Non-hex character `%c` found in postscript hex string",
                c
            );
        }
    }

    if (first_pass) {
        string_out->data =
            arena_alloc(tokenizer->arena, sizeof(uint8_t) * string_out->len);
        tokenizer->offset = restore_offset;
        return read_hex_string(tokenizer, string_out, false);
    } else {
        return NULL;
    }
}

Error*
ps_next_token(PSTokenizer* tokenizer, PSToken* token_out, bool* got_token) {
    RELEASE_ASSERT(tokenizer);
    RELEASE_ASSERT(token_out);

    // Consume comments and whitespace
    while (consume_comment(tokenizer)) {
    }

    consume_whitespace(tokenizer);

    // Determine token type
    if (tokenizer->offset >= tokenizer->data_len) {
        *got_token = false;
        return NULL;
    } else {
        *got_token = true;
    }

    size_t start_offset = tokenizer->offset;

    uint8_t c = tokenizer->data[tokenizer->offset++];
    if (c == '+' || c == '-') {
        TRY(read_number_or_executable_name(
            tokenizer,
            tokenizer->offset - 1,
            c == '-',
            true,
            false,
            token_out
        ));
    } else if (c == '.') {
        TRY(read_number_or_executable_name(
            tokenizer,
            tokenizer->offset - 1,
            false,
            false,
            true,
            token_out
        ));
    } else if (c >= '0' && c <= '9') {
        tokenizer->offset--;
        TRY(read_number_or_executable_name(
            tokenizer,
            tokenizer->offset,
            false,
            false,
            false,
            token_out
        ));
    } else if (c == '(') {
        token_out->type = PS_TOKEN_LIT_STRING;
        TRY(read_lit_string(tokenizer, &token_out->data.string, true));
    } else if (c == '<') {
        if (tokenizer->offset >= tokenizer->data_len) {
            return ERROR(PS_ERR_EOF, "EOF after start of hex string.");
        }

        uint8_t c2 = tokenizer->data[tokenizer->offset];
        if (c2 == '<') {
            tokenizer->offset++;
            token_out->type = PS_TOKEN_START_DICT;
        } else if (c2 == '~') {
            LOG_TODO("Base-85 postscript strings");
        } else {
            token_out->type = PS_TOKEN_HEX_STRING;
            TRY(read_hex_string(tokenizer, &token_out->data.string, true));
        }
    } else if (c == '/') {
        if (tokenizer->offset >= tokenizer->data_len) {
            token_out->type = PS_TOKEN_LIT_NAME;
            token_out->data.name = "";
        } else {
            uint8_t c2 = tokenizer->data[tokenizer->offset];
            if (c2 == '/') {
                tokenizer->offset++;
                token_out->type = PS_TOKEN_IMM_NAME;
                token_out->data.name = read_name(tokenizer, 0);
            } else {
                token_out->type = PS_TOKEN_LIT_NAME;
                token_out->data.name = read_name(tokenizer, 0);
            }
        }
    } else if (c == '[') {
        token_out->type = PS_TOKEN_START_ARRAY;
    } else if (c == ']') {
        token_out->type = PS_TOKEN_END_ARRAY;
    } else if (c == '{') {
        token_out->type = PS_TOKEN_START_PROC;
    } else if (c == '}') {
        token_out->type = PS_TOKEN_END_PROC;
    } else if (c == '>') {
        if (tokenizer->offset >= tokenizer->data_len) {
            return ERROR(PS_ERR_EOF, "Hit EOF when expecting `>`");
        }

        uint8_t c2 = tokenizer->data[tokenizer->offset++];
        if (c2 != '>') {
            return ERROR(PS_ERR_INVALID_CHAR, "Expected `>`");
        }

        token_out->type = PS_TOKEN_END_DICT;
    } else if (is_postscript_regular(c)) {
        token_out->type = PS_TOKEN_EXE_NAME;
        token_out->data.name = read_name(tokenizer, 1);
    } else {
        return ERROR(
            PS_ERR_INVALID_CHAR,
            "Unexpected character `%c` in postscript",
            c
        );
    }

    RELEASE_ASSERT(tokenizer->offset >= start_offset);
    return NULL;
}

char* ps_string_as_cstr(PSString string, Arena* arena) {
    RELEASE_ASSERT(arena);

    if (string.len == 0) {
        return "";
    }

    char* cstr = arena_alloc(arena, sizeof(char) * (string.len + 1));
    memcpy(cstr, string.data, string.len);
    cstr[string.len] = '\0';

    return cstr;
}

Error* ps_string_as_uint(PSString string, uint64_t* out_value) {
    RELEASE_ASSERT(out_value);

    if (string.len == 0) {
        *out_value = 0;
        return NULL;
    }

    if (string.len > 8) {
        // Check high bytes for non-zero values, which would be an overflow
        for (size_t idx = 0; idx < string.len - 8; idx++) {
            if (string.data[idx] != 0) {
                return ERROR(
                    PS_ERR_LIMITCHECK,
                    "Failed to convert hex string to 64-bit unsigned integer"
                );
            }
        }
    }

    *out_value = 0;
    for (size_t idx = 0; idx < string.len; idx++) {
        *out_value <<= 8;
        *out_value |= (uint64_t)string.data[idx];
    }

    return NULL;
}

#ifdef TEST

#include <string.h>

#include "test/test.h"

#define SETUP_TOKENIZER(text)                                                  \
    uint8_t buffer[] = text;                                                   \
    Arena* arena = arena_new(1024);                                            \
    PSTokenizer* tokenizer =                                                   \
        ps_tokenizer_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1); \
    PSToken token;                                                             \
    bool got_token = false;

#define CHECK_STREAM_CONSUMED()                                                \
    TEST_REQUIRE(ps_next_token(tokenizer, &token, &got_token));                \
    TEST_ASSERT(!got_token);                                                   \
    TEST_ASSERT_EQ(tokenizer->data_len, tokenizer->offset);                    \
    arena_free(arena);

#define GET_TOKEN_NO_DATA(expected_type)                                       \
    TEST_REQUIRE(ps_next_token(tokenizer, &token, &got_token));                \
    TEST_ASSERT(got_token, "Expected token");                                  \
    TEST_ASSERT_EQ(                                                            \
        (int)(expected_type),                                                  \
        (int)(token.type),                                                     \
        "Incorrect token type"                                                 \
    );

#define GET_TOKEN_WITH_DATA(expected_type, data_field, expected_value)         \
    GET_TOKEN_NO_DATA(expected_type)                                           \
    TEST_ASSERT_EQ(                                                            \
        expected_value,                                                        \
        token.data.data_field,                                                 \
        "Incorrect token value"                                                \
    );

#define GET_STR_TOKEN_WITH_DATA(expected_type, expected_value)                 \
    GET_TOKEN_NO_DATA(expected_type)                                           \
    TEST_ASSERT_EQ(                                                            \
        expected_value,                                                        \
        ps_string_as_cstr(token.data.string, arena),                           \
        "Incorrect token value"                                                \
    );

// TODO: test 37 radix, 0 radix, missing trailing digit for radix num,
// limitcheck real, limitcheck hex, base-85 strings

TEST_FUNC(test_ps_tokenize_integer) {
    SETUP_TOKENIZER("123")
    GET_TOKEN_WITH_DATA(PS_TOKEN_INTEGER, integer, 123)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_pos_integer) {
    SETUP_TOKENIZER("+123")
    GET_TOKEN_WITH_DATA(PS_TOKEN_INTEGER, integer, 123)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_neg_integer) {
    SETUP_TOKENIZER("-123")
    GET_TOKEN_WITH_DATA(PS_TOKEN_INTEGER, integer, -123)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_int_max) {
    SETUP_TOKENIZER("2147483647")
    GET_TOKEN_WITH_DATA(PS_TOKEN_INTEGER, integer, 2147483647)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_int_min) {
    SETUP_TOKENIZER("-2147483648")
    GET_TOKEN_WITH_DATA(PS_TOKEN_INTEGER, integer, -2147483647 - 1)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_pos_real_conversion) {
    SETUP_TOKENIZER("2147483648")
    GET_TOKEN_WITH_DATA(PS_TOKEN_REAL, real, 2147483648.0)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_neg_real_conversion) {
    SETUP_TOKENIZER("-2147483649")
    GET_TOKEN_WITH_DATA(PS_TOKEN_REAL, real, -2147483649.0)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_real) {
    SETUP_TOKENIZER("34.5")
    GET_TOKEN_WITH_DATA(PS_TOKEN_REAL, real, 34.5)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_real_trailing_zeros) {
    SETUP_TOKENIZER("34.5000")
    GET_TOKEN_WITH_DATA(PS_TOKEN_REAL, real, 34.5)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_pos_real) {
    SETUP_TOKENIZER("+34.5")
    GET_TOKEN_WITH_DATA(PS_TOKEN_REAL, real, 34.5)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_neg_real) {
    SETUP_TOKENIZER("-34.5")
    GET_TOKEN_WITH_DATA(PS_TOKEN_REAL, real, -34.5)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_no_leading_real) {
    SETUP_TOKENIZER(".5")
    GET_TOKEN_WITH_DATA(PS_TOKEN_REAL, real, 0.5)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_no_trailing_real) {
    SETUP_TOKENIZER("5.")
    GET_TOKEN_WITH_DATA(PS_TOKEN_REAL, real, 5.0)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_neg_no_leading_real) {
    SETUP_TOKENIZER("-.002")
    GET_TOKEN_WITH_DATA(PS_TOKEN_REAL, real, -0.002)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_exp_dot_real) {
    SETUP_TOKENIZER("123.6e3")
    GET_TOKEN_WITH_DATA(PS_TOKEN_REAL, real, 123.6e3)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_exp_real) {
    SETUP_TOKENIZER("1E6")
    GET_TOKEN_WITH_DATA(PS_TOKEN_REAL, real, 1e6)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_neg_exp_real) {
    SETUP_TOKENIZER("1e-3")
    GET_TOKEN_WITH_DATA(PS_TOKEN_REAL, real, 1e-3)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_oct_radix) {
    SETUP_TOKENIZER("8#1777")
    GET_TOKEN_WITH_DATA(PS_TOKEN_RADIX_NUM, integer, 1023)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_radix_neg) {
    SETUP_TOKENIZER("16#fffffffe")
    GET_TOKEN_WITH_DATA(PS_TOKEN_RADIX_NUM, integer, -2)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_radix_uppercase) {
    SETUP_TOKENIZER("16#ffFfFFFE")
    GET_TOKEN_WITH_DATA(PS_TOKEN_RADIX_NUM, integer, -2)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_bin_radix) {
    SETUP_TOKENIZER("2#100")
    GET_TOKEN_WITH_DATA(PS_TOKEN_RADIX_NUM, integer, 4)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_radix_single_digit) {
    SETUP_TOKENIZER("16#a")
    GET_TOKEN_WITH_DATA(PS_TOKEN_RADIX_NUM, integer, 10)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_lit_string) {
    SETUP_TOKENIZER("(This is a string.)")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_LIT_STRING, "This is a string.")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_lit_string_newline) {
    SETUP_TOKENIZER("(Strings may contain newlines\nand such.)")
    GET_STR_TOKEN_WITH_DATA(
        PS_TOKEN_LIT_STRING,
        "Strings may contain newlines\nand such."
    )
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_lit_string_special_chars) {
    SETUP_TOKENIZER("(Strings may contain special characters *!&}^%.)")
    GET_STR_TOKEN_WITH_DATA(
        PS_TOKEN_LIT_STRING,
        "Strings may contain special characters *!&}^%."
    )
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_lit_string_balanced_parens) {
    SETUP_TOKENIZER(
        "(Strings may contain balanced parenthesis () (and so on ()))"
    )
    GET_STR_TOKEN_WITH_DATA(
        PS_TOKEN_LIT_STRING,
        "Strings may contain balanced parenthesis () (and so on ())"
    )
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_lit_string_empty) {
    SETUP_TOKENIZER("()")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_LIT_STRING, "")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_lit_string_escapes) {
    SETUP_TOKENIZER("(\\n\\r\\t\\b\\f\\\\\\)\\()")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_LIT_STRING, "\n\r\t\b\f\\)(")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_lit_string_octal) {
    SETUP_TOKENIZER("(\\0053\\53\\053)")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_LIT_STRING, "\0053\053\053")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_lit_string_escape_ignore) {
    SETUP_TOKENIZER("(Hello\\ World)")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_LIT_STRING, "Hello World")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_lit_string_escape_newline) {
    SETUP_TOKENIZER(
        "(Hello\\\n World\\\r\n This is a test\\\r This is also a test)"
    )
    GET_STR_TOKEN_WITH_DATA(
        PS_TOKEN_LIT_STRING,
        "Hello World This is a test This is also a test"
    )
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_hex_string) {
    SETUP_TOKENIZER("<68656C6C6F20776F726C64>")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_HEX_STRING, "hello world")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_hex_string_spaces) {
    SETUP_TOKENIZER("< 686  56  \r\fC6C6F\n207\t76F 726C6 4>")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_HEX_STRING, "hello world")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_hex_string_even_len) {
    SETUP_TOKENIZER("<901fa3>")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_HEX_STRING, "\x90\x1f\xa3")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_hex_string_odd_len) {
    SETUP_TOKENIZER("<901fa>")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_HEX_STRING, "\x90\x1f\xa0")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_hex_string_unexpected) {
    SETUP_TOKENIZER("<90x3>")

    TEST_REQUIRE_ERR(
        ps_next_token(tokenizer, &token, &got_token),
        PS_ERR_INVALID_CHAR
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_name) {
    SETUP_TOKENIZER("abc")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "abc")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_name_non_alpha) {
    SETUP_TOKENIZER("$@a_3name#")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "$@a_3name#")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_leading_num_name) {
    SETUP_TOKENIZER("23a")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "23a")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_leading_real_name) {
    SETUP_TOKENIZER("23.54a")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "23.54a")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_leading_real_name2) {
    SETUP_TOKENIZER(".54a")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, ".54a")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_leading_real_name3) {
    SETUP_TOKENIZER("-.54a")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "-.54a")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_leading_exp_name) {
    SETUP_TOKENIZER(".54e5a")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, ".54e5a")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_leading_exp_name2) {
    SETUP_TOKENIZER("54e5a")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "54e5a")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_leading_radix_name) {
    SETUP_TOKENIZER("2#101a")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "2#101a")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_leading_radix_name2) {
    SETUP_TOKENIZER("2#1012")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "2#1012")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

// Since 37 is an invalid radix, it should fall back to a name
TEST_FUNC(test_ps_tokenize_invalid_radix_name) {
    SETUP_TOKENIZER("37#101")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "37#101")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

// "A real number consists of an optional sign and one or more decimal
// digits..." This has no decimal digits, therefore is not a real, it is a name.
TEST_FUNC(test_ps_tokenize_single_dot_name) {
    SETUP_TOKENIZER(".")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, ".")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_single_plus_name) {
    SETUP_TOKENIZER("+")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "+")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_single_plus_name_non_eof) {
    SETUP_TOKENIZER("+ ")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "+")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_plus_dot_name) {
    SETUP_TOKENIZER("+.")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "+.")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_leading_signed_num_name) {
    SETUP_TOKENIZER("+51a")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "+51a")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_lit_name) {
    SETUP_TOKENIZER("/name")
    GET_TOKEN_WITH_DATA(PS_TOKEN_LIT_NAME, name, "name")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_empty_lit_name) {
    SETUP_TOKENIZER("/")
    GET_TOKEN_WITH_DATA(PS_TOKEN_LIT_NAME, name, "")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_num_lit_name) {
    SETUP_TOKENIZER("/1")
    GET_TOKEN_WITH_DATA(PS_TOKEN_LIT_NAME, name, "1")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_lit_name_delim_term) {
    SETUP_TOKENIZER("/name()")
    GET_TOKEN_WITH_DATA(PS_TOKEN_LIT_NAME, name, "name")

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_imm_name) {
    SETUP_TOKENIZER("//name")
    GET_TOKEN_WITH_DATA(PS_TOKEN_IMM_NAME, name, "name")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_array) {
    SETUP_TOKENIZER("[ 123 /abc (xyz) ]")
    GET_TOKEN_NO_DATA(PS_TOKEN_START_ARRAY)
    GET_TOKEN_WITH_DATA(PS_TOKEN_INTEGER, integer, 123)
    GET_TOKEN_WITH_DATA(PS_TOKEN_LIT_NAME, name, "abc")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_LIT_STRING, "xyz")
    GET_TOKEN_NO_DATA(PS_TOKEN_END_ARRAY)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_array_compact) {
    SETUP_TOKENIZER("[123/abc(xyz)]")
    GET_TOKEN_NO_DATA(PS_TOKEN_START_ARRAY)
    GET_TOKEN_WITH_DATA(PS_TOKEN_INTEGER, integer, 123)
    GET_TOKEN_WITH_DATA(PS_TOKEN_LIT_NAME, name, "abc")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_LIT_STRING, "xyz")
    GET_TOKEN_NO_DATA(PS_TOKEN_END_ARRAY)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_proc) {
    SETUP_TOKENIZER("{add 2 div}")
    GET_TOKEN_NO_DATA(PS_TOKEN_START_PROC)
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "add")
    GET_TOKEN_WITH_DATA(PS_TOKEN_INTEGER, integer, 2)
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "div")
    GET_TOKEN_NO_DATA(PS_TOKEN_END_PROC)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_tokenize_dict) {
    SETUP_TOKENIZER("<<key1 4 /key2 (value)>>")
    GET_TOKEN_NO_DATA(PS_TOKEN_START_DICT)
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "key1")
    GET_TOKEN_WITH_DATA(PS_TOKEN_INTEGER, integer, 4)
    GET_TOKEN_WITH_DATA(PS_TOKEN_LIT_NAME, name, "key2")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_LIT_STRING, "value")
    GET_TOKEN_NO_DATA(PS_TOKEN_END_DICT)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ps_comments) {
    SETUP_TOKENIZER("  % This is a comment\ntest  (test2)\n3 % test")
    GET_TOKEN_WITH_DATA(PS_TOKEN_EXE_NAME, name, "test")
    GET_STR_TOKEN_WITH_DATA(PS_TOKEN_LIT_STRING, "test2")
    GET_TOKEN_WITH_DATA(PS_TOKEN_INTEGER, integer, 3)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

#endif // TEST
