#pragma once

typedef enum {
    PDF_OK,
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
    PDF_ERR_SCHEMA_UNKNOWN_KEY,
    PDF_ERR_SCHEMA_DUPLICATE_KEY,
    PDF_ERR_SCHEMA_INCORRECT_TYPE,
    PDF_CTX_ERR_EOF,
    PDF_CTX_ERR_EXPECT,
    PDF_CTX_ERR_SCAN_LIMIT,
    PDF_CTX_ERR_BORROWED,
    PDF_CTX_ERR_NOT_BORROWED,
} PdfResult;

#define PDF_TRY(op)                                                            \
    do {                                                                       \
        PdfResult try_result = (op);                                           \
        if (try_result != PDF_OK) {                                            \
            return try_result;                                                 \
        }                                                                      \
    } while (0)

#define PDF_TRY_RET_NULL(op)                                                   \
    do {                                                                       \
        PdfResult try_result = (op);                                           \
        if (try_result != PDF_OK) {                                            \
            if (result) {                                                      \
                *result = try_result;                                          \
            }                                                                  \
            return NULL;                                                       \
        }                                                                      \
    } while (0)
