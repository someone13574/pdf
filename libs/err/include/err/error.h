#pragma once

#include <stdbool.h>

#include "../../common/include/attributes.h"

typedef enum {
    CFF_ERR_EOF,
    CFF_ERR_EXPECTED_OPERATOR,
    CFF_ERR_INCORRECT_OPERAND,
    CFF_ERR_INVALID_CHARSET,
    CFF_ERR_INVALID_FONT,
    CFF_ERR_INVALID_INDEX,
    CFF_ERR_INVALID_OBJECT_IDX,
    CFF_ERR_INVALID_OFFSET_SIZE,
    CFF_ERR_INVALID_OPERATOR,
    CFF_ERR_INVALID_REAL_OPERAND,
    CFF_ERR_INVALID_SID,
    CFF_ERR_INVALID_SUBR,
    CFF_ERR_MISSING_OPERAND,
    CFF_ERR_RESERVED,
    CFF_ERR_UNSUPPORTED_VERSION,
    CODEC_ERR_BITSTREAM_EOD,
    CODEC_ERR_DEFLATE_BACKREF_UNDERFLOW,
    CODEC_ERR_DEFLATE_INVALID_BLOCK_TYPE,
    CODEC_ERR_DEFLATE_INVALID_FIXED_HUFFMAN,
    CODEC_ERR_DEFLATE_INVALID_SYMBOL,
    CODEC_ERR_DEFLATE_LEN_COMPLIMENT,
    CODEC_ERR_DEFLATE_REPEAT_OVERFLOW,
    CODEC_ERR_DEFLATE_REPEAT_UNDERFLOW,
    CODEC_ERR_ZLIB_INVALID_CHECKSUM,
    CODEC_ERR_ZLIB_INVALID_CM,
    CODEC_ERR_ZLIB_INVALID_FCHECK,
    CODEC_ERR_ZLIB_RESERVED_CM,
    PDF_ERR_ASCII_HEX_INVALID,
    PDF_ERR_CMAP_ALREADY_DERIVE,
    PDF_ERR_CMAP_INVALID_CODEPOINT,
    PDF_ERR_CMAP_INVALID_GIA_LEN,
    PDF_ERR_CMAP_INVALID_PLATFORM,
    PDF_ERR_CTX_BORROWED,
    PDF_ERR_CTX_EOF,
    PDF_ERR_CTX_EXPECT,
    PDF_ERR_CTX_NOT_BORROWED,
    PDF_ERR_CTX_SCAN_LIMIT,
    PDF_ERR_DUPLICATE_KEY,
    PDF_ERR_EXCESS_OPERAND,
    PDF_ERR_INCORRECT_TYPE,
    PDF_ERR_INVALID_CID,
    PDF_ERR_INVALID_GLYPH_NAME,
    PDF_ERR_INVALID_NUMBER,
    PDF_ERR_INVALID_OBJECT,
    PDF_ERR_INVALID_OPERAND_DESCRIPTOR,
    PDF_ERR_INVALID_STARTXREF,
    PDF_ERR_INVALID_SUBTYPE,
    PDF_ERR_INVALID_TRAILER,
    PDF_ERR_INVALID_VERSION,
    PDF_ERR_INVALID_XREF,
    PDF_ERR_INVALID_XREF_REFERENCE,
    PDF_ERR_MISSING_DICT_KEY,
    PDF_ERR_MISSING_OPERAND,
    PDF_ERR_NAME_BAD_CHAR_CODE,
    PDF_ERR_NAME_UNESCAPED_CHAR,
    PDF_ERR_NO_PAGES,
    PDF_ERR_NUMBER_LIMIT,
    PDF_ERR_OBJECT_NOT_DICT,
    PDF_ERR_STREAM_INVALID_LENGTH,
    PDF_ERR_UNBALANCED_STR,
    PDF_ERR_UNIMPLEMENTED_KEY,
    PDF_ERR_UNKNOWN_CMAP,
    PDF_ERR_UNKNOWN_KEY,
    PDF_ERR_UNKNOWN_OPERATOR,
    PDF_ERR_XREF_GENERATION_MISMATCH,
    PS_ERR_ACCESS_VIOLATION,
    PS_ERR_ARRAY_NOT_STARTED,
    PS_ERR_EOF,
    PS_ERR_INVALID_CHAR,
    PS_ERR_INVALID_LENGTH,
    PS_ERR_KEY_MISSING,
    PS_ERR_LIMITCHECK,
    PS_ERR_OPERANDS_EMPTY,
    PS_ERR_OPERAND_TYPE,
    PS_ERR_POP_STANDARD_DICT,
    PS_ERR_RESOURCE_DEFINED,
    PS_ERR_UNKNOWN_RESOURCE,
    PS_ERR_USER_DATA_INVALID,
    RENDER_ERR_FONT_NOT_SET,
    RENDER_ERR_GSTATE_CANNOT_RESTORE,
    SFNT_ERR_BAD_HEAD,
    SFNT_ERR_BAD_MAGIC,
    SFNT_ERR_EOF,
    SFNT_ERR_INVALID_GID,
    SFNT_ERR_INVALID_LENGTH,
    SFNT_ERR_INVALID_VERSION,
    SFNT_ERR_MISSING_TABLE,
    SFNT_ERR_RESERVED,
    SFNT_ERR_TABLE_CHECKSUM,
    CTX_EOF
} ErrorCode;

typedef struct Error Error;

Error* error_new(ErrorCode code) RET_NONNULL_ATTR;
void error_free(Error* error);

Error* error_conditional_context(Error* error, Error* context_error);

Error* error_add_context(
    Error* error,
    const char* func,
    const char* file,
    unsigned long line,
    const char* fmt,
    ...
) RET_NONNULL_ATTR FORMAT_ATTR(5, 6);
ErrorCode error_code(const Error* error);

void error_print(const Error* error);
void error_unwrap(
    Error* error,
    const char* file,
    unsigned long line
) NORETURN_ATTR;
bool error_free_is_ok(Error* error);

#if defined(SOURCE_PATH_SIZE)
#define RELATIVE_FILE_PATH (&__FILE__[SOURCE_PATH_SIZE])
#else
#define RELATIVE_FILE_PATH __FILE__
#endif

#define ERROR_ADD_CONTEXT(err)                                                 \
    error_add_context((err), __func__, RELATIVE_FILE_PATH, __LINE__, NULL)

#define ERROR_ADD_CONTEXT_FMT(err, ...)                                        \
    error_add_context(                                                         \
        (err),                                                                 \
        __func__,                                                              \
        RELATIVE_FILE_PATH,                                                    \
        __LINE__,                                                              \
        __VA_ARGS__                                                            \
    )

#define _ERR_CAT_INNER(a, b) a##b
#define _ERR_CAT(a, b) _ERR_CAT_INNER(a, b)

#define TRY(expr, ...)                                                         \
    do {                                                                       \
        Error* try_error_internal = (expr);                                    \
        if (try_error_internal) {                                              \
            return _ERR_CAT(ERROR_ADD_CONTEXT, __VA_OPT__(_FMT))(              \
                try_error_internal __VA_OPT__(, __VA_ARGS__)                   \
            );                                                                 \
        }                                                                      \
    } while (0)

#define REQUIRE(expr, ...)                                                     \
    do {                                                                       \
        Error* require_error_internal = (expr);                                \
        if (require_error_internal) {                                          \
            error_unwrap(                                                      \
                _ERR_CAT(ERROR_ADD_CONTEXT, __VA_OPT__(_FMT))(                 \
                    require_error_internal __VA_OPT__(, __VA_ARGS__)           \
                ),                                                             \
                RELATIVE_FILE_PATH,                                            \
                __LINE__                                                       \
            );                                                                 \
        }                                                                      \
    } while (0)

#define ERROR(code, ...)                                                       \
    _ERR_CAT(ERROR_ADD_CONTEXT, __VA_OPT__(_FMT))                              \
    (error_new(code) __VA_OPT__(, __VA_ARGS__))

#ifdef TEST

#define TEST_REQUIRE(expr)                                                     \
    do {                                                                       \
        Error* require_error_internal = (expr);                                \
        if (require_error_internal) {                                          \
            error_print(require_error_internal);                               \
            LOG_ERROR(                                                         \
                TEST,                                                          \
                "An error occurred during the test: %d",                       \
                error_code(require_error_internal)                             \
            );                                                                 \
            error_free(require_error_internal);                                \
            return TEST_RESULT_FAIL;                                           \
        }                                                                      \
    } while (0)

#define TEST_REQUIRE_ERR(expr, code)                                                \
    do {                                                                            \
        Error* require_error_internal = (expr);                                     \
        if (!require_error_internal) {                                              \
            LOG_ERROR(TEST, "Expected an error of type " #code " to occur");        \
            return TEST_RESULT_FAIL;                                                \
        }                                                                           \
        if (error_code(require_error_internal) != (code)) {                         \
            LOG_ERROR(                                                              \
                TEST,                                                               \
                "Expression returned the incorrect error code. Expected %d (" #code \
                "), found %d",                                                      \
                error_code(require_error_internal),                                 \
                (code)                                                              \
            );                                                                      \
            error_free(require_error_internal);                                     \
            return TEST_RESULT_FAIL;                                                \
        }                                                                           \
        error_free(require_error_internal);                                         \
    } while (0)

#endif // TEST
