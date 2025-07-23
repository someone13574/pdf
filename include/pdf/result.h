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
    PDF_ERR_SFNT_EOF,
    PDF_ERR_SFNT_MISSING_TABLE,
    PDF_ERR_SFNT_TABLE_CHECKSUM,
    PDF_ERR_CMAP_INVALID_PLATFORM,
    PDF_ERR_CMAP_INVALID_GIA_LEN,
    PDF_ERR_CMAP_RESERVED_PAD,
    PDF_ERR_SFNT_BAD_MAGIC
} PdfResult;

#define PDF_PROPAGATE(op)                                                      \
    do {                                                                       \
        PdfResult try_result = (op);                                           \
        if (try_result != PDF_OK) {                                            \
            return try_result;                                                 \
        }                                                                      \
    } while (0)

#define PDF_REQUIRE(op)                                                        \
    do {                                                                       \
        PdfResult result = (op);                                               \
        if (result != PDF_OK) {                                                \
            LOG_PANIC("Error code %d occurred", result);                       \
        }                                                                      \
    } while (0)
