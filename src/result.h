#pragma once

typedef enum {
    PDF_OK,
    PDF_ERR_INVALID_VERSION,
    PDF_ERR_INVALID_TRAILER,
    PDF_ERR_INVALID_XREF,
    PDF_ERR_INVALID_XREF_REFERENCE,
    PDF_ERR_XREF_GENERATION_MISMATCH,
    PDF_CTX_ERR_EOF,
    PDF_CTX_ERR_EXPECT,
    PDF_CTX_ERR_SCAN_LIMIT,
    PDF_CTX_ERR_BORROWED,
    PDF_CTX_ERR_NOT_BORROWED,
    PDF_CTX_ERR_INVALID_NUMBER,
} PdfResult;

#define PDF_TRY(op)                                                            \
    do {                                                                       \
        PdfResult try_result = (op);                                           \
        if (try_result != PDF_OK) {                                            \
            return try_result;                                                 \
        }                                                                      \
    } while (0)
