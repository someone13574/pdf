#include "object.h"

#include <ctype.h>
#include <stdint.h>

#include "arena.h"
#include "ctx.h"
#include "log.h"
#include "result.h"

PdfObject* pdf_parse_true(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_false(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_null(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_number(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject*
pdf_parse_string_literal(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_name(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_indirect(Arena* arena, PdfCtx* ctx, PdfResult* result);

PdfObject* pdf_parse_object(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    DBG_ASSERT(arena);
    DBG_ASSERT(ctx);
    DBG_ASSERT(result);

    LOG_DEBUG_G("object", "Parsing object at offset %zu", pdf_ctx_offset(ctx));

    *result = PDF_OK;

    char peeked, peeked_next;
    PDF_TRY_RET_NULL(pdf_ctx_peek(ctx, &peeked));

    PdfResult peeked_next_result = pdf_ctx_peek_next(ctx, &peeked_next);
    if (peeked == '<' && peeked_next_result != PDF_OK) {
        *result = peeked_next_result;
        return NULL;
    }

    if (peeked == 't') {
        return pdf_parse_true(arena, ctx, result);
    } else if (peeked == 'f') {
        return pdf_parse_false(arena, ctx, result);
    } else if (peeked == '.' || peeked == '+' || peeked == '-') {
        return pdf_parse_number(arena, ctx, result);
    } else if (isdigit((unsigned char)peeked)) {
        size_t restore_offset = pdf_ctx_offset(ctx);
        PdfObject* indirect = pdf_parse_indirect(arena, ctx, result);
        if (*result == PDF_OK) {
            return indirect;
        }

        *result = PDF_OK;
        PDF_TRY_RET_NULL(pdf_ctx_seek(ctx, restore_offset));

        return pdf_parse_number(arena, ctx, result);
    } else if (peeked == '(') {
        return pdf_parse_string_literal(arena, ctx, result);
    } else if (peeked == '<' && peeked_next != '<') {
        LOG_TODO("Hexadecimal strings");
    } else if (peeked == '/') {
        return pdf_parse_name(arena, ctx, result);
    } else if (peeked == '[') {
        LOG_TODO("Arrays");
    } else if (peeked == '<' && peeked_next == '<') {
        LOG_TODO("Dictionaries");
    } else if (peeked == 's') {
        LOG_TODO("Streams");
    } else if (peeked == 'n') {
        return pdf_parse_null(arena, ctx, result);
    }

    *result = PDF_ERR_INVALID_OBJECT;
    return NULL;
}

PdfObject* pdf_parse_true(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "true"));

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->kind = PDF_OBJECT_KIND_BOOLEAN;
    object->data.bool_data = true;

    return object;
}

PdfObject* pdf_parse_false(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "false"));

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->kind = PDF_OBJECT_KIND_BOOLEAN;
    object->data.bool_data = false;

    return object;
}

PdfObject* pdf_parse_null(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "null"));

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->kind = PDF_OBJECT_KIND_NULL;

    return object;
}

PdfObject* pdf_parse_number(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    LOG_TRACE_G("object", "Parsing number at offset %zu", pdf_ctx_offset(ctx));

    // Parse sign
    char sign_char;
    int32_t sign = 1;
    PDF_TRY_RET_NULL(pdf_ctx_peek(ctx, &sign_char));
    if (sign_char == '+' || sign_char == '-') {
        PDF_TRY_RET_NULL(pdf_ctx_peek_and_advance(ctx, NULL));

        if (sign_char == '-') {
            sign = -1;
        }
    }

    // Parse leading digits
    char digit_char;
    int64_t leading_acc = 0;
    bool has_leading = false;

    while (pdf_ctx_peek(ctx, &digit_char) == PDF_OK && isdigit(digit_char)) {
        has_leading = true;
        int64_t digit = digit_char - '0';

        if (leading_acc <= (INT64_MAX - digit) / 10) {
            leading_acc *= 10;
            leading_acc += digit;
        } else {
            leading_acc = INT64_MAX;
        }

        PDF_TRY_RET_NULL(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    // Parse decimal point
    char decimal_char;
    PdfResult decimal_peek_result = pdf_ctx_peek(ctx, &decimal_char);

    if ((decimal_peek_result == PDF_OK && decimal_char != '.')
        || decimal_peek_result == PDF_CTX_ERR_EOF) {
        LOG_TRACE_G("object", "Number is integer");

        if (!has_leading) {
            *result = PDF_ERR_INVALID_NUMBER;
            return NULL;
        }

        leading_acc *= sign;
        if (leading_acc < (int64_t)INT32_MIN
            || leading_acc > (int64_t)INT32_MAX) {
            *result = PDF_ERR_NUMBER_LIMIT;
            return NULL;
        }

        PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
        object->kind = PDF_OBJECT_KIND_INTEGER;
        object->data.integer_data = (int32_t)leading_acc;
        return object;
    } else if (decimal_peek_result != PDF_OK) {
        *result = decimal_peek_result;
        return NULL;
    } else {
        PDF_TRY_RET_NULL(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    LOG_TRACE_G("object", "Number is real");

    // Parse trailing digits
    double trailing_acc = 0.0;
    double trailing_weight = 0.1;
    bool has_trailing = false;

    while (pdf_ctx_peek(ctx, &digit_char) == PDF_OK && isdigit(digit_char)) {
        has_trailing = true;

        trailing_acc += (double)(digit_char - '0') * trailing_weight;
        trailing_weight *= 0.1;

        PDF_TRY_RET_NULL(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    double value = ((double)leading_acc + trailing_acc) * (double)sign;
    const double PDF_FLOAT_MAX = 3.403e38;

    if (value > PDF_FLOAT_MAX || value < -PDF_FLOAT_MAX) {
        *result = PDF_ERR_NUMBER_LIMIT;
        return NULL;
    }

    if (!has_leading && !has_trailing) {
        *result = PDF_ERR_INVALID_NUMBER;
        return NULL;
    }

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->kind = PDF_OBJECT_KIND_REAL;
    object->data.real_data = value;

    return object;
}

PdfObject*
pdf_parse_string_literal(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "("));

    // Find length upper bound
    char peeked_char;
    int escape = 0;
    int open_parenthesis = 1;
    size_t start_offset = pdf_ctx_offset(ctx);

    while (open_parenthesis > 0
           && pdf_ctx_peek_and_advance(ctx, &peeked_char) == PDF_OK) {
        if (peeked_char == '(' && escape == 0) {
            open_parenthesis++;
        } else if (peeked_char == ')' && escape == 0) {
            open_parenthesis--;
        }

        if (peeked_char == '\\' && escape == 0) {
            escape = 1;
        } else {
            escape = 0;
        }
    }

    if (open_parenthesis != 0) {
        *result = PDF_ERR_UNBALANCED_STR;
        return NULL;
    }

    // Parse
    char* raw;
    PDF_TRY_RET_NULL(pdf_ctx_borrow_substr(
        ctx,
        start_offset,
        pdf_ctx_offset(ctx) - start_offset - 1,
        &raw
    ));

    char* parsed =
        arena_alloc(arena, sizeof(char) * (pdf_ctx_offset(ctx) - start_offset));

    escape = 0;
    size_t write_offset = 0;

    for (size_t read_offset = 0;
         read_offset < pdf_ctx_offset(ctx) - start_offset - 1;
         read_offset++) {
        char read_char = raw[read_offset];

        switch (escape) {
            case 0: {
                if (read_char == '\\') {
                    escape = 1;
                    break;
                }

                parsed[write_offset++] = read_char;
                break;
            }
            case 1: {
                switch (read_char) {
                    case 'n': {
                        parsed[write_offset++] = '\n';
                        escape = 0;
                        break;
                    }
                    case 'r': {
                        parsed[write_offset++] = '\r';
                        escape = 0;
                        break;
                    }
                    case 't': {
                        parsed[write_offset++] = '\t';
                        escape = 0;
                        break;
                    }
                    case 'b': {
                        parsed[write_offset++] = '\b';
                        escape = 0;
                        break;
                    }
                    case 'f': {
                        parsed[write_offset++] = '\f';
                        escape = 0;
                        break;
                    }
                    case '(': {
                        parsed[write_offset++] = '(';
                        escape = 0;
                        break;
                    }
                    case ')': {
                        parsed[write_offset++] = ')';
                        escape = 0;
                        break;
                    }
                    case '\\': {
                        parsed[write_offset++] = '\\';
                        escape = 0;
                        break;
                    }
                    default: {
                        LOG_TODO("Octal escape codes and split lines");
                    }
                }
                break;
            }
            default: {
                LOG_PANIC("Unreachable");
            }
        }
    }

    PDF_TRY_RET_NULL(pdf_ctx_release_substr(ctx));
    parsed[write_offset] = '\0';

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->kind = PDF_OBJECT_KIND_STRING;
    object->data.string_data = parsed;

    return object;
}

bool char_to_hex(char c, int* out) {
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

PdfObject* pdf_parse_name(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "/"));

    // Find max length
    size_t start_offset = pdf_ctx_offset(ctx);
    size_t length = 0;
    char peeked;

    while (pdf_ctx_peek_and_advance(ctx, &peeked) == PDF_OK) {
        if (!is_pdf_regular(peeked)) {
            break;
        }

        if (peeked < '!' || peeked > '~') {
            *result = PDF_ERR_NAME_UNESCAPED_CHAR;
            return NULL;
        }

        length++;
    }

    LOG_DEBUG("Length %zu", length);

    // Parse name
    char* name = arena_alloc(arena, sizeof(char) * (length + 1));
    char* raw;
    PDF_TRY_RET_NULL(pdf_ctx_borrow_substr(ctx, start_offset, length, &raw));

    size_t write_offset = 0;
    int escape = 0;
    int hex_code = 0;

    for (size_t read_offset = 0; read_offset < length; read_offset++) {
        char c = raw[read_offset];

        switch (escape) {
            case 0: {
                if (c == '#') {
                    escape = 1;
                    continue;
                }

                name[write_offset++] = c;
                break;
            }
            case 1: {
                int value;
                if (!char_to_hex(c, &value)) {
                    *result = PDF_ERR_NAME_BAD_CHAR_CODE;
                    return NULL;
                }

                hex_code = value << 4;
                escape = 2;
                break;
            }
            case 2: {
                int value;
                if (!char_to_hex(c, &value)) {
                    *result = PDF_ERR_NAME_BAD_CHAR_CODE;
                    return NULL;
                }

                hex_code |= value;
                name[write_offset++] = (char)hex_code;
                escape = 0;
                break;
            }
            default: {
                LOG_PANIC("Unreachable");
            }
        }
    }

    PDF_TRY_RET_NULL(pdf_ctx_release_substr(ctx));
    name[write_offset] = '\0';

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->kind = PDF_OBJECT_KIND_NAME;
    object->data.name_data = name;

    return object;
}

PdfObject* pdf_parse_indirect(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    LOG_DEBUG_G("object", "Parsing indirect object or reference");

    // Parse object id
    uint64_t object_id;
    uint32_t int_length;
    PDF_TRY_RET_NULL(pdf_ctx_parse_int(ctx, NULL, &object_id, &int_length));
    if (int_length == 0) {
        *result = PDF_CTX_ERR_EXPECT;
        return NULL;
    }

    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, " "));

    // Parse generation
    uint64_t generation;
    PDF_TRY_RET_NULL(pdf_ctx_parse_int(ctx, NULL, &generation, &int_length));
    if (int_length == 0) {
        *result = PDF_CTX_ERR_EXPECT;
        return NULL;
    }

    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, " "));

    // Determine if indirect object or reference
    char peeked;
    PDF_TRY_RET_NULL(pdf_ctx_peek(ctx, &peeked));

    if (peeked == 'R') {
        LOG_DEBUG_G("object", "Parsed indirect reference");
        PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
        object->kind = PDF_OBJECT_KIND_REF;
        object->data.ref_data.object_id = object_id;
        object->data.ref_data.generation = generation;

        return object;
    } else {
        LOG_DEBUG_G("object", "Parsed indirect object");

        PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "obj"));
        PDF_TRY_RET_NULL(pdf_ctx_consume_whitespace(ctx));

        PdfObject* inner = pdf_parse_object(arena, ctx, result);
        if (*result != PDF_OK) {
            LOG_DEBUG_G(
                "object",
                "Failed to parsing inner of indirect object with result %d",
                *result
            );
            return NULL;
        }

        PDF_TRY_RET_NULL(pdf_ctx_consume_whitespace(ctx));
        PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "endobj"));

        PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
        object->kind = PDF_OBJECT_KIND_INDIRECT;
        object->data.indirect_data.object = inner;
        object->data.indirect_data.object_id = object_id;
        object->data.indirect_data.generation = generation;

        return object;
    }

    return NULL;
}

#ifdef TEST
#include <string.h>

#include "test.h"

#define SETUP_VALID_PARSE_OBJECT(buf, type)                                    \
    Arena* arena = arena_new(128);                                             \
    char buffer[] = buf;                                                       \
    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));                  \
    PdfResult result;                                                          \
    PdfObject* object = pdf_parse_object(arena, ctx, &result);                 \
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);                                 \
    TEST_ASSERT(object);                                                       \
    TEST_ASSERT_EQ((PdfObjectKind)(type), object->kind);

#define SETUP_INVALID_PARSE_OBJECT(buf, err)                                   \
    Arena* arena = arena_new(128);                                             \
    char buffer[] = buf;                                                       \
    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));                  \
    PdfResult result;                                                          \
    PdfObject* object = pdf_parse_object(arena, ctx, &result);                 \
    TEST_ASSERT_EQ((PdfResult)(err), result);                                  \
    TEST_ASSERT(!object);

TEST_FUNC(test_object_bool_true) {
    SETUP_VALID_PARSE_OBJECT("true", PDF_OBJECT_KIND_BOOLEAN);
    TEST_ASSERT(object->data.bool_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_bool_false) {
    SETUP_VALID_PARSE_OBJECT("false", PDF_OBJECT_KIND_BOOLEAN);
    TEST_ASSERT(!object->data.bool_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_null) {
    SETUP_VALID_PARSE_OBJECT("null", PDF_OBJECT_KIND_NULL);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_123) {
    SETUP_VALID_PARSE_OBJECT("123", PDF_OBJECT_KIND_INTEGER);
    TEST_ASSERT_EQ((int32_t)123, object->data.integer_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_plus_17) {
    SETUP_VALID_PARSE_OBJECT("+17", PDF_OBJECT_KIND_INTEGER);
    TEST_ASSERT_EQ((int32_t)17, object->data.integer_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_minus_98) {
    SETUP_VALID_PARSE_OBJECT("-98", PDF_OBJECT_KIND_INTEGER);
    TEST_ASSERT_EQ((int32_t)-98, object->data.integer_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_0) {
    SETUP_VALID_PARSE_OBJECT("0", PDF_OBJECT_KIND_INTEGER);
    TEST_ASSERT_EQ((int32_t)0, object->data.integer_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_large) {
    SETUP_VALID_PARSE_OBJECT("2147483647", PDF_OBJECT_KIND_INTEGER);
    TEST_ASSERT_EQ((int32_t)2147483647, object->data.integer_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_neg_large) {
    SETUP_VALID_PARSE_OBJECT("-2147483648", PDF_OBJECT_KIND_INTEGER);
    TEST_ASSERT_EQ((int32_t)-2147483648, object->data.integer_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_overflow) {
    SETUP_INVALID_PARSE_OBJECT("2147483648", PDF_ERR_NUMBER_LIMIT);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_neg_overflow) {
    SETUP_INVALID_PARSE_OBJECT("-2147483649", PDF_ERR_NUMBER_LIMIT);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_plus_no_digits) {
    SETUP_INVALID_PARSE_OBJECT("+", PDF_ERR_INVALID_NUMBER);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_minus_no_digits) {
    SETUP_INVALID_PARSE_OBJECT("-", PDF_ERR_INVALID_NUMBER);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_34_5) {
    SETUP_VALID_PARSE_OBJECT("34.5", PDF_OBJECT_KIND_REAL);
    TEST_ASSERT_EQ_EPS((double)34.5, object->data.real_data, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_minus_3_62) {
    SETUP_VALID_PARSE_OBJECT("-3.62", PDF_OBJECT_KIND_REAL);
    TEST_ASSERT_EQ_EPS((double)-3.62, object->data.real_data, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_plus_123_6) {
    SETUP_VALID_PARSE_OBJECT("123.6", PDF_OBJECT_KIND_REAL);
    TEST_ASSERT_EQ_EPS((double)123.6, object->data.real_data, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_4_) {
    SETUP_VALID_PARSE_OBJECT("4.", PDF_OBJECT_KIND_REAL);
    TEST_ASSERT_EQ_EPS((double)4.0, object->data.real_data, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_minus_dot_002) {
    SETUP_VALID_PARSE_OBJECT("-.002", PDF_OBJECT_KIND_REAL);
    TEST_ASSERT_EQ_EPS((double)-0.002, object->data.real_data, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_0_0) {
    SETUP_VALID_PARSE_OBJECT("0.0", PDF_OBJECT_KIND_REAL);
    TEST_ASSERT_EQ_EPS((double)0.0, object->data.real_data, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_no_digits) {
    SETUP_INVALID_PARSE_OBJECT(".", PDF_ERR_INVALID_NUMBER);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str) {
    SETUP_VALID_PARSE_OBJECT("(This is a string)", PDF_OBJECT_KIND_STRING);
    TEST_ASSERT_EQ("This is a string", object->data.string_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_newline) {
    SETUP_VALID_PARSE_OBJECT(
        "(Strings may contain newlines\nand such.)",
        PDF_OBJECT_KIND_STRING
    );
    TEST_ASSERT_EQ(
        "Strings may contain newlines\nand such.",
        object->data.string_data
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_parenthesis) {
    SETUP_VALID_PARSE_OBJECT(
        "(Strings may contain balanced parentheses () and special characters (*!&^% and so on).)",
        PDF_OBJECT_KIND_STRING
    );
    TEST_ASSERT_EQ(
        "Strings may contain balanced parentheses () and special characters (*!&^% and so on).",
        object->data.string_data
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_empty) {
    SETUP_VALID_PARSE_OBJECT("()", PDF_OBJECT_KIND_STRING);
    TEST_ASSERT_EQ("", object->data.string_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_escapes) {
    SETUP_VALID_PARSE_OBJECT(
        "(This is a line\\nThis is a newline\\r\\nThis is another line with tabs (\\t), backspaces(\\b), form feeds (\\f), unbalanced parenthesis \\(\\(\\(\\), and backslashes \\\\\\\\)",
        PDF_OBJECT_KIND_STRING
    );
    TEST_ASSERT_EQ(
        "This is a line\nThis is a newline\r\nThis is another line with tabs (\t), backspaces(\b), form feeds (\f), unbalanced parenthesis (((), and backslashes \\\\",
        object->data.string_data
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_unbalanced) {
    SETUP_INVALID_PARSE_OBJECT("(", PDF_ERR_UNBALANCED_STR);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name) {
    SETUP_VALID_PARSE_OBJECT("/Name1", PDF_OBJECT_KIND_NAME);
    TEST_ASSERT_EQ("Name1", object->data.name_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_various_chars) {
    SETUP_VALID_PARSE_OBJECT(
        "/A;Name_With-Various***Characters?",
        PDF_OBJECT_KIND_NAME
    );
    TEST_ASSERT_EQ("A;Name_With-Various***Characters?", object->data.name_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_dollars) {
    SETUP_VALID_PARSE_OBJECT("/$$", PDF_OBJECT_KIND_NAME);
    TEST_ASSERT_EQ("$$", object->data.name_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_escape) {
    SETUP_VALID_PARSE_OBJECT(
        "/lime#20Green",
        PDF_OBJECT_KIND_NAME
    ); // PDF 32000-1:2008 has a typo for this example
    TEST_ASSERT_EQ("lime Green", object->data.name_data);
    TEST_ASSERT_NE("Lime Green", object->data.name_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_terminated) {
    SETUP_VALID_PARSE_OBJECT("/paired#28#29parentheses/", PDF_OBJECT_KIND_NAME);
    TEST_ASSERT_EQ("paired()parentheses", object->data.name_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_whitespace_terminated) {
    SETUP_VALID_PARSE_OBJECT("/A#42 ", PDF_OBJECT_KIND_NAME);
    TEST_ASSERT_EQ("AB", object->data.name_data);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_invalid) {
    SETUP_INVALID_PARSE_OBJECT(
        "/The_Key_of_F#_Minor",
        PDF_ERR_NAME_BAD_CHAR_CODE
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_indirect) {
    SETUP_VALID_PARSE_OBJECT(
        "12 0 obj (Brillig) endobj",
        PDF_OBJECT_KIND_INDIRECT
    );
    TEST_ASSERT_EQ((size_t)12, object->data.indirect_data.object_id);
    TEST_ASSERT_EQ((size_t)0, object->data.indirect_data.generation);
    TEST_ASSERT_EQ(
        (PdfObjectKind)PDF_OBJECT_KIND_STRING,
        object->data.indirect_data.object->kind
    );
    TEST_ASSERT_EQ(
        "Brillig",
        object->data.indirect_data.object->data.string_data
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_indirect_nested) {
    SETUP_VALID_PARSE_OBJECT(
        "12 0 obj 54 3 obj /Name endobj endobj",
        PDF_OBJECT_KIND_INDIRECT
    );
    TEST_ASSERT_EQ((size_t)12, object->data.indirect_data.object_id);
    TEST_ASSERT_EQ((size_t)0, object->data.indirect_data.generation);

    TEST_ASSERT_EQ(
        (PdfObjectKind)PDF_OBJECT_KIND_INDIRECT,
        object->data.indirect_data.object->kind
    );
    TEST_ASSERT_EQ(
        (size_t)54,
        object->data.indirect_data.object->data.indirect_data.object_id
    );
    TEST_ASSERT_EQ(
        (size_t)3,
        object->data.indirect_data.object->data.indirect_data.generation
    );

    TEST_ASSERT_EQ(
        (PdfObjectKind)PDF_OBJECT_KIND_NAME,
        object->data.indirect_data.object->data.indirect_data.object->kind
    );
    TEST_ASSERT_EQ(
        "Name",
        object->data.indirect_data.object->data.indirect_data.object->data
            .string_data
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_indirect_ref) {
    SETUP_VALID_PARSE_OBJECT("12 0 R", PDF_OBJECT_KIND_REF);
    TEST_ASSERT_EQ((size_t)12, object->data.indirect_data.object_id);
    TEST_ASSERT_EQ((size_t)0, object->data.indirect_data.generation);

    return TEST_RESULT_PASS;
}

#endif // TEST
