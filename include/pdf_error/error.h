#pragma once

#include <stdbool.h>

#include "attributes.h"

typedef enum {
    PDF_ERR_INVALID_VERSION,
    PDF_ERR_INVALID_TRAILER,
    PDF_ERR_INVALID_STARTXREF,
    PDF_ERR_INVALID_XREF,
    PDF_ERR_INVALID_XREF_REFERENCE,
    PDF_ERR_XREF_GENERATION_MISMATCH,
    PDF_ERR_INVALID_OBJECT,
    PDF_ERR_INVALID_NUMBER,
    PDF_ERR_NUMBER_LIMIT,
    PDF_ERR_UNBALANCED_STR,
    PDF_ERR_NAME_UNESCAPED_CHAR,
    PDF_ERR_NAME_BAD_CHAR_CODE,
    PDF_ERR_STREAM_INVALID_LENGTH,
    PDF_ERR_OBJECT_NOT_DICT,
    PDF_ERR_MISSING_DICT_KEY,
    PDF_ERR_UNKNOWN_KEY,
    PDF_ERR_DUPLICATE_KEY,
    PDF_ERR_INCORRECT_TYPE,
    PDF_ERR_UNKNOWN_OPERATOR,
    PDF_ERR_MISSING_OPERAND,
    PDF_ERR_EXCESS_OPERAND,
    PDF_ERR_INVALID_OPERAND_DESCRIPTOR,
    PDF_ERR_CTX_EOF,
    PDF_ERR_CTX_EXPECT,
    PDF_ERR_CTX_SCAN_LIMIT,
    PDF_ERR_CTX_BORROWED,
    PDF_ERR_CTX_NOT_BORROWED,
    PDF_ERR_FILTER_ASCII_HEX_INVALID,
    PDF_ERR_SFNT_EOF,
    PDF_ERR_SFNT_MISSING_TABLE,
    PDF_ERR_SFNT_TABLE_CHECKSUM,
    PDF_ERR_CMAP_INVALID_PLATFORM,
    PDF_ERR_CMAP_INVALID_GIA_LEN,
    PDF_ERR_SFNT_RESERVED,
    PDF_ERR_SFNT_INVALID_VERSION,
    PDF_ERR_SFNT_BAD_MAGIC,
    PDF_ERR_SFNT_BAD_HEAD,
    PDF_ERR_SFNT_INVALID_GID,
    PDF_ERR_DEFLATE_LEN_COMPLIMENT,
    PDF_ERR_DEFLATE_INVALID_FIXED_HUFFMAN,
    PDF_ERR_DEFLATE_INVALID_BLOCK_TYPE,
    PDF_ERR_CODEC_BITSTREAM_EOD
} PdfErrorCode;

typedef struct PdfError PdfError;

PdfError* pdf_error_new(PdfErrorCode code) RET_NONNULL_ATTR;
void pdf_error_free(PdfError* error);

PdfError* pdf_error_add_context(
    PdfError* error,
    const char* func,
    const char* file,
    unsigned long line,
    const char* fmt,
    ...
) RET_NONNULL_ATTR FORMAT_ATTR(5, 6);
PdfErrorCode pdf_error_code(PdfError* error);

void pdf_error_print(PdfError* error);
void pdf_error_unwrap(PdfError* error, const char* file, unsigned long line)
    NORETURN_ATTR;
bool pdf_error_free_is_ok(PdfError* error);

#if defined(SOURCE_PATH_SIZE)
#define RELATIVE_FILE_PATH (&__FILE__[SOURCE_PATH_SIZE])
#else
#define RELATIVE_FILE_PATH __FILE__
#endif

#define PDF_ERROR_ADD_CONTEXT(err)                                             \
    pdf_error_add_context((err), __func__, RELATIVE_FILE_PATH, __LINE__, NULL)

#define PDF_ERROR_ADD_CONTEXT_FMT(err, ...)                                    \
    pdf_error_add_context(                                                     \
        (err),                                                                 \
        __func__,                                                              \
        RELATIVE_FILE_PATH,                                                    \
        __LINE__,                                                              \
        __VA_ARGS__                                                            \
    )

#define _PDF_ERR_CAT_INNER(a, b) a##b
#define _PDF_ERR_CAT(a, b) _PDF_ERR_CAT_INNER(a, b)

#define PDF_PROPAGATE(expr, ...)                                               \
    do {                                                                       \
        PdfError* pdf_error = (expr);                                          \
        if (pdf_error) {                                                       \
            return _PDF_ERR_CAT(PDF_ERROR_ADD_CONTEXT, __VA_OPT__(_FMT))(      \
                pdf_error __VA_OPT__(, __VA_ARGS__)                            \
            );                                                                 \
        }                                                                      \
    } while (0)

#define PDF_REQUIRE(expr, ...)                                                 \
    do {                                                                       \
        PdfError* pdf_error = (expr);                                          \
        if (pdf_error) {                                                       \
            pdf_error_unwrap(                                                  \
                _PDF_ERR_CAT(PDF_ERROR_ADD_CONTEXT, __VA_OPT__(_FMT))(         \
                    pdf_error __VA_OPT__(, __VA_ARGS__)                        \
                ),                                                             \
                RELATIVE_FILE_PATH,                                            \
                __LINE__                                                       \
            );                                                                 \
        }                                                                      \
    } while (0)

#define PDF_ERROR(code, ...)                                                   \
    _PDF_ERR_CAT(PDF_ERROR_ADD_CONTEXT, __VA_OPT__(_FMT))                      \
    (pdf_error_new(code) __VA_OPT__(, __VA_ARGS__))

#ifdef TEST

#define TEST_PDF_REQUIRE(expr)                                                 \
    do {                                                                       \
        PdfError* error = (expr);                                              \
        if (error) {                                                           \
            pdf_error_print(error);                                            \
            LOG_ERROR(                                                         \
                TEST,                                                          \
                "An error occurred during the test: %d",                       \
                pdf_error_code(error)                                          \
            );                                                                 \
            pdf_error_free(error);                                             \
            return TEST_RESULT_FAIL;                                           \
        }                                                                      \
    } while (0)

#define TEST_PDF_REQUIRE_ERR(expr, code)                                                      \
    do {                                                                                      \
        PdfError* error = (expr);                                                             \
        if (!error) {                                                                         \
            LOG_ERROR(TEST, "Expected an error of type " #code " to occur");                  \
            return TEST_RESULT_FAIL;                                                          \
        }                                                                                     \
        if (pdf_error_code(error) != (code)) {                                                \
            LOG_ERROR(                                                                        \
                TEST,                                                                         \
                "Expression returned the incorrect error code. Expected %d, found %d (" #code \
                ")",                                                                          \
                pdf_error_code(error),                                                        \
                (code)                                                                        \
            );                                                                                \
            pdf_error_free(error);                                                            \
            return TEST_RESULT_FAIL;                                                          \
        }                                                                                     \
        pdf_error_free(error);                                                                \
    } while (0)

#endif // TEST
