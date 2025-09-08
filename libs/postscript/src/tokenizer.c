#include "postscript/tokenizer.h"

#include <stdbool.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "pdf_error/error.h"

struct PostscriptTokenizer {
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
        LOG_TODO("Integer or real");
    } else if (c == '.') {
        LOG_TODO("Real");
    } else if (c >= '0' && c <= '9') {
        LOG_TODO("Integer, real, radix number, or executable name");
    } else if (c == '(') {
        LOG_TODO("Literal string");
    } else if (c == '<') {
        LOG_TODO("Hex string, base-85 string, or start dictionary");
    } else if (c == '/') {
        LOG_TODO("Literal or immediately evaluated name");
    } else if (c == '[') {
        LOG_TODO("Start array");
    } else if (c == ']') {
        LOG_TODO("End array");
    } else if (c == '{') {
        LOG_TODO("Start procedure");
    } else if (c == '}') {
        LOG_TODO("End procedure");
    } else if (c == '>') {
        LOG_TODO("End dictionary");
    } else if (is_postscript_regular(c)) {
        LOG_TODO("Executable name");
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

#define GET_TOKEN_WITH_DATA(expected_type, data_field, expected_value)         \
    TEST_PDF_REQUIRE(postscript_next_token(tokenizer, &token, &got_token));    \
    TEST_ASSERT(got_token, "Expected token");                                  \
    TEST_ASSERT_EQ(                                                            \
        (int)(expected_type),                                                  \
        (int)(token.type),                                                     \
        "Incorrect token type"                                                 \
    );                                                                         \
    TEST_ASSERT_EQ(                                                            \
        expected_value,                                                        \
        token.data.data_field,                                                 \
        "Incorrect token value"                                                \
    );

#define GET_STR_TOKEN_WITH_DATA(expected_type, expected_value)                 \
    TEST_PDF_REQUIRE(postscript_next_token(tokenizer, &token, &got_token));    \
    TEST_ASSERT(got_token, "Expected token");                                  \
    TEST_ASSERT_EQ(                                                            \
        (int)(expected_type),                                                  \
        (int)(token.type),                                                     \
        "Incorrect token type"                                                 \
    );                                                                         \
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
        "Strings may contain balanced parenthesis () (and so on)"
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

#endif // TEST
