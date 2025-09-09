#include "postscript/tokenizer.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "pdf_error/error.h"

struct PostscriptTokenizer {
    Arena* arena;

    const char* data;
    size_t data_len;
    size_t offset;
};

PostscriptTokenizer*
postscript_tokenizer_new(Arena* arena, const char* data, size_t data_len) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(data);

    PostscriptTokenizer* tokenizer =
        arena_alloc(arena, sizeof(PostscriptTokenizer));
    tokenizer->arena = arena;
    tokenizer->data = data;
    tokenizer->data_len = data_len;
    tokenizer->offset = 0;

    return tokenizer;
}

static bool is_postscript_whitespace(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\0'
        || c == '\f';
}

static bool is_postscript_delimiter(char c) {
    return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']'
        || c == '{' || c == '}' || c == '/' || c == '%';
}

static bool is_postscript_regular(char c) {
    return !is_postscript_whitespace(c) && !is_postscript_delimiter(c);
}

static void consume_whitespace(PostscriptTokenizer* tokenizer) {
    RELEASE_ASSERT(tokenizer);

    while (tokenizer->offset < tokenizer->data_len) {
        char c = tokenizer->data[tokenizer->offset];
        if (is_postscript_whitespace(c)) {
            tokenizer->offset++;
        } else {
            break;
        }
    }
}

static bool consume_comment(PostscriptTokenizer* tokenizer) {
    RELEASE_ASSERT(tokenizer);

    consume_whitespace(tokenizer);

    if (tokenizer->offset >= tokenizer->data_len
        || tokenizer->data[tokenizer->offset] != '%') {
        return false;
    }

    while (tokenizer->offset < tokenizer->data_len) {
        char c = tokenizer->data[tokenizer->offset++];
        if (c == '\n') {
            break;
        } else if (c == '\r') {
            consume_whitespace(tokenizer);
            break;
        }
    }

    return true;
}

static char* read_name(PostscriptTokenizer* tokenizer, size_t known_length) {
    RELEASE_ASSERT(tokenizer);

    // Measure length
    while (tokenizer->offset < tokenizer->data_len) {
        char c = tokenizer->data[tokenizer->offset];

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

static PdfError* read_lit_string(
    PostscriptTokenizer* tokenizer,
    PostscriptString* string_out,
    bool first_pass
) {
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
        return PDF_ERROR(PDF_ERR_POSTSCRIPT_EOF);
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

PdfError* postscript_next_token(
    PostscriptTokenizer* tokenizer,
    PostscriptToken* token_out,
    bool* got_token
) {
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

    char c = tokenizer->data[tokenizer->offset++];
    if (c == '+' || c == '-') {
        LOG_TODO("Integer, real, or executable name");
    } else if (c == '.') {
        LOG_TODO("Real");
    } else if (c >= '0' && c <= '9') {
        LOG_TODO("Integer, real, radix number, or executable name");
    } else if (c == '(') {
        token_out->type = POSTSCRIPT_TOKEN_LIT_STRING;
        return read_lit_string(tokenizer, &token_out->data.string, true);
    } else if (c == '<') {
        LOG_TODO("Hex string, base-85 string, or start dictionary");
    } else if (c == '/') {
        if (tokenizer->offset >= tokenizer->data_len) {
            token_out->type = POSTSCRIPT_TOKEN_LIT_NAME;
            token_out->data.name = "";

            return NULL;
        }

        char c2 = tokenizer->data[tokenizer->offset];
        if (c2 == '/') {
            tokenizer->offset++;
            token_out->type = POSTSCRIPT_TOKEN_IMM_NAME;
            token_out->data.name = read_name(tokenizer, 0);
        } else {
            token_out->type = POSTSCRIPT_TOKEN_LIT_NAME;
            token_out->data.name = read_name(tokenizer, 0);
        }
    } else if (c == '[') {
        token_out->type = POSTSCRIPT_TOKEN_START_ARRAY;
    } else if (c == ']') {
        token_out->type = POSTSCRIPT_TOKEN_END_ARRAY;
    } else if (c == '{') {
        token_out->type = POSTSCRIPT_TOKEN_START_PROC;
    } else if (c == '}') {
        token_out->type = POSTSCRIPT_TOKEN_END_PROC;
    } else if (c == '>') {
        if (tokenizer->offset >= tokenizer->data_len) {
            return PDF_ERROR(
                PDF_ERR_POSTSCRIPT_EOF,
                "Hit EOF when expecting `>`"
            );
        }

        char c2 = tokenizer->data[tokenizer->offset++];
        if (c2 != '>') {
            return PDF_ERROR(PDF_ERR_POSTSCRIPT_INVALID_CHAR, "Expected `>`");
        }

        token_out->type = POSTSCRIPT_TOKEN_END_DICT;
    } else if (is_postscript_regular(c)) {
        token_out->type = POSTSCRIPT_TOKEN_EXE_NAME;
        token_out->data.name = read_name(tokenizer, 1);
    } else {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_INVALID_CHAR,
            "Unexpected character `%c` in postscript",
            c
        );
    }

    return NULL;
}

#ifdef TEST

#include <string.h>

#include "test/test.h"

static char* ps_string_as_cstr(PostscriptString string, Arena* arena) {
    RELEASE_ASSERT(arena);

    char* cstr = arena_alloc(arena, sizeof(char) * (string.len + 1));
    memcpy(cstr, string.data, string.len);
    cstr[string.len] = '\0';

    return cstr;
}

#define SETUP_TOKENIZER(text)                                                  \
    char buffer[] = text;                                                      \
    Arena* arena = arena_new(1024);                                            \
    PostscriptTokenizer* tokenizer = postscript_tokenizer_new(                 \
        arena,                                                                 \
        buffer,                                                                \
        sizeof(buffer) / sizeof(char)                                          \
    );                                                                         \
    PostscriptToken token;                                                     \
    bool got_token = false;

#define CHECK_STREAM_CONSUMED()                                                \
    TEST_PDF_REQUIRE(postscript_next_token(tokenizer, &token, &got_token));    \
    TEST_ASSERT(!got_token);                                                   \
    TEST_ASSERT_EQ(tokenizer->data_len, tokenizer->offset);                    \
    arena_free(arena);

#define GET_TOKEN_NO_DATA(expected_type)                                       \
    TEST_PDF_REQUIRE(postscript_next_token(tokenizer, &token, &got_token));    \
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

TEST_FUNC(test_postscript_tokenize_integer) {
    SETUP_TOKENIZER("123")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_INTEGER, integer, 123)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_pos_integer) {
    SETUP_TOKENIZER("+123")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_INTEGER, integer, 123)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_neg_integer) {
    SETUP_TOKENIZER("-123")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_INTEGER, integer, -123)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_int_max) {
    SETUP_TOKENIZER("2147483647")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_INTEGER, integer, 2147483647)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_int_min) {
    SETUP_TOKENIZER("-2147483648")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_INTEGER, integer, -2147483647 - 1)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_pos_real_conversion) {
    SETUP_TOKENIZER("2147483648")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_REAL, real, 2147483648.0)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_neg_real_conversion) {
    SETUP_TOKENIZER("-2147483649")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_REAL, real, -2147483649.0)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_real) {
    SETUP_TOKENIZER("34.5")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_REAL, real, 34.5)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_pos_real) {
    SETUP_TOKENIZER("+34.5")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_REAL, real, 34.5)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_neg_real) {
    SETUP_TOKENIZER("-34.5")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_REAL, real, -34.5)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_no_leading_real) {
    SETUP_TOKENIZER(".5")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_REAL, real, 0.5)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_no_trailing_real) {
    SETUP_TOKENIZER("5.")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_REAL, real, 5.)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_neg_no_leading_real) {
    SETUP_TOKENIZER("-.002")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_REAL, real, -0.002)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_exp_dot_real) {
    SETUP_TOKENIZER("123.6e3")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_REAL, real, 123.6e3)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_exp_real) {
    SETUP_TOKENIZER("1E6")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_REAL, real, 1e6)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_neg_exp_real) {
    SETUP_TOKENIZER("1e-3")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_REAL, real, 1e-3)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_oct_radix) {
    SETUP_TOKENIZER("8#1777")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_RADIX_NUM, integer, 1023)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_radix_neg) {
    SETUP_TOKENIZER("16#fffe")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_RADIX_NUM, integer, -2)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_radix_uppercase) {
    SETUP_TOKENIZER("16#FFFE")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_RADIX_NUM, integer, -2)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_bin_radix) {
    SETUP_TOKENIZER("2#100")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_RADIX_NUM, integer, 4)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_radix_single_digit) {
    SETUP_TOKENIZER("16#a")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_RADIX_NUM, integer, 10)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_lit_string) {
    SETUP_TOKENIZER("(This is a string.)")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_STRING, "This is a string.")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_lit_string_newline) {
    SETUP_TOKENIZER("(Strings may contain newlines\nand such.)")
    GET_STR_TOKEN_WITH_DATA(
        POSTSCRIPT_TOKEN_LIT_STRING,
        "Strings may contain newlines\nand such."
    )
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_lit_string_special_chars) {
    SETUP_TOKENIZER("(Strings may contain special characters *!&}^%.)")
    GET_STR_TOKEN_WITH_DATA(
        POSTSCRIPT_TOKEN_LIT_STRING,
        "Strings may contain special characters *!&}^%."
    )
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_lit_string_balanced_parens) {
    SETUP_TOKENIZER(
        "(Strings may contain balanced parenthesis () (and so on ()))"
    )
    GET_STR_TOKEN_WITH_DATA(
        POSTSCRIPT_TOKEN_LIT_STRING,
        "Strings may contain balanced parenthesis () (and so on ())"
    )
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_lit_string_empty) {
    SETUP_TOKENIZER("()")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_STRING, "")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_lit_string_escapes) {
    SETUP_TOKENIZER("(\\n\\r\\t\\b\\f\\\\\\)\\()")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_STRING, "\n\r\t\b\f\\)(")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_lit_string_octal) {
    SETUP_TOKENIZER("(\\0053\\53\\053)")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_STRING, "\0053\053\053")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_lit_string_escape_ignore) {
    SETUP_TOKENIZER("(Hello\\ World)")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_STRING, "Hello World")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_lit_string_escape_newline) {
    SETUP_TOKENIZER(
        "(Hello\\\n World\\\r\n This is a test\\\r This is also a test)"
    )
    GET_STR_TOKEN_WITH_DATA(
        POSTSCRIPT_TOKEN_LIT_STRING,
        "Hello World This is a test This is also a test"
    )
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_hex_string) {
    SETUP_TOKENIZER("<68656C6C6F20776F726C64>")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_HEX_STRING, "hello world")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_hex_string_spaces) {
    SETUP_TOKENIZER("< 686  56  \r\fC6C6F\n207\t76F 726C6 4>")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_HEX_STRING, "hello world")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_hex_string_even_len) {
    SETUP_TOKENIZER("<901fa3>")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_HEX_STRING, "\x90\x1f\xa3")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_hex_string_odd_len) {
    SETUP_TOKENIZER("<901fa>")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_HEX_STRING, "\x90\x1f\xa0")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_name) {
    SETUP_TOKENIZER("abc")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_EXE_NAME, name, "abc")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_name_non_alpha) {
    SETUP_TOKENIZER("$@a_3name#")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_EXE_NAME, name, "$@a_name#")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_leading_nums) {
    SETUP_TOKENIZER("23a")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_EXE_NAME, name, "23a")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_leading_signed_num) {
    SETUP_TOKENIZER("+51a")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_EXE_NAME, name, "+51a")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_lit_name) {
    SETUP_TOKENIZER("/name")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_NAME, name, "name")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_empty_lit_name) {
    SETUP_TOKENIZER("/")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_NAME, name, "")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_num_lit_name) {
    SETUP_TOKENIZER("/1")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_NAME, name, "1")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_lit_name_delim_term) {
    SETUP_TOKENIZER("/name()")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_NAME, name, "name")

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_imm_name) {
    SETUP_TOKENIZER("//name")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_IMM_NAME, name, "name")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_array) {
    SETUP_TOKENIZER("[ 123 /abc (xyz) ]")
    GET_TOKEN_NO_DATA(POSTSCRIPT_TOKEN_START_ARRAY)
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_INTEGER, integer, 123)
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_NAME, name, "abc")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_STRING, "xyz")
    GET_TOKEN_NO_DATA(POSTSCRIPT_TOKEN_END_ARRAY)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_array_minspace) {
    SETUP_TOKENIZER("[123/abc(xyz)]")
    GET_TOKEN_NO_DATA(POSTSCRIPT_TOKEN_START_ARRAY)
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_INTEGER, integer, 123)
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_NAME, name, "abc")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_STRING, "xyz")
    GET_TOKEN_NO_DATA(POSTSCRIPT_TOKEN_END_ARRAY)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_proc) {
    SETUP_TOKENIZER("{add 2 div}")
    GET_TOKEN_NO_DATA(POSTSCRIPT_TOKEN_START_PROC)
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_EXE_NAME, name, "add")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_INTEGER, integer, 2)
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_EXE_NAME, name, "div")
    GET_TOKEN_NO_DATA(POSTSCRIPT_TOKEN_END_PROC)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_tokenize_dict) {
    SETUP_TOKENIZER("<<key1 4 /key2 (value)>>")
    GET_TOKEN_NO_DATA(POSTSCRIPT_TOKEN_START_DICT)
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_EXE_NAME, name, "key1")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_INTEGER, integer, 4)
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_NAME, name, "key2")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_STRING, "value")
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_postscript_comments) {
    SETUP_TOKENIZER("  % This is a comment\ntest  (test2)\n3 % test")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_EXE_NAME, name, "test")
    GET_STR_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_LIT_STRING, "test2")
    GET_TOKEN_WITH_DATA(POSTSCRIPT_TOKEN_INTEGER, integer, 3)
    CHECK_STREAM_CONSUMED()

    return TEST_RESULT_PASS;
}

#endif // TEST
