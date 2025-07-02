#pragma once

typedef enum {
    PDF_OK,
    PDF_ERR_INVALID_VERSION,
    PDF_ERR_INVALID_TRAILER,
    PDF_CTX_ERR_EOF,
    PDF_CTX_ERR_EXPECT,
    PDF_CTX_ERR_SCAN_LIMIT,
    PDF_CTX_ERR_BORROWED,
    PDF_CTX_ERR_NOT_BORROWED
} PdfResult;

#define PDF_TRY(op)                                                            \
    do {                                                                       \
        PdfResult try_result = (op);                                           \
        if (try_result != PDF_OK) {                                            \
            return try_result;                                                 \
        }                                                                      \
    } while (0)
