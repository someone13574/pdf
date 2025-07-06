#include "object.h"

#include <ctype.h>
#include <stdint.h>

#include "arena.h"
#include "ctx.h"
#include "log.h"
#include "result.h"

PdfObject* pdf_parse_true(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_false(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_number(Arena* arena, PdfCtx* ctx, PdfResult* result);

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
        LOG_WARN("could be indirect object/ref");
        return pdf_parse_number(arena, ctx, result);
    } else if (peeked == '(') {
        // literal string
        LOG_TODO();
    } else if (peeked == '<' && peeked_next != '<') {
        // hex string
        LOG_TODO();
    } else if (peeked == '/') {
        // name
        LOG_TODO();
    } else if (peeked == '[') {
        // array
        LOG_TODO();
    } else if (peeked == '<' && peeked_next == '<') {
        // dict
        LOG_TODO();
    } else if (peeked == 's') {
        // stream
        LOG_TODO();
    } else if (peeked == 'n') {
        // null
        LOG_TODO();
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
    bool has_leading = false;
    int64_t leading_acc = 0;

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

    while (pdf_ctx_peek(ctx, &digit_char) == PDF_OK && isdigit(digit_char)) {
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

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->kind = PDF_OBJECT_KIND_REAL;
    object->data.real_data = value;

    return object;
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
    TEST_ASSERT(object);                                                       \
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);                                 \
    TEST_ASSERT_EQ((PdfObjectKind)(type), object->kind);

#define SETUP_INVALID_PARSE_OBJECT(buf, err)                                   \
    Arena* arena = arena_new(128);                                             \
    char buffer[] = buf;                                                       \
    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));                  \
    PdfResult result;                                                          \
    PdfObject* object = pdf_parse_object(arena, ctx, &result);                 \
    TEST_ASSERT(!object);                                                      \
    TEST_ASSERT_EQ((PdfResult)(err), result);

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

#endif // TEST
