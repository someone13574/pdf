#include "pdf_error/error.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "logger/log.h"

typedef struct PdfErrorCtx {
    struct PdfErrorCtx* next;

    char* message;
    const char* func;
    const char* file;
    unsigned long line;
} PdfErrorCtx;

static PdfErrorCtx* pdf_error_ctx_extend(
    PdfErrorCtx* chain,
    char* message,
    const char* func,
    const char* file,
    unsigned long line
) {
    RELEASE_ASSERT(func);
    RELEASE_ASSERT(file);

    PdfErrorCtx* ctx = malloc(sizeof(PdfErrorCtx));
    RELEASE_ASSERT(ctx);

    ctx->next = chain;
    ctx->message = message;
    ctx->func = func;
    ctx->file = file;
    ctx->line = line;

    return ctx;
}

static void pdf_error_ctx_display(PdfErrorCtx* chain) {
    RELEASE_ASSERT(chain);

    if (chain->next) {
        pdf_error_ctx_display(chain->next);
    }

    if (chain->message) {
        logger_log(
            "ERROR",
            LOG_SEVERITY_ERROR,
            LOG_DIAG_VERBOSITY_INFO,
            LOG_DIAG_VERBOSITY_TRACE,
            chain->file,
            chain->line,
            "Error context: func=`%s`, msg=\"%s\"",
            chain->func,
            chain->message
        );
    } else {
        logger_log(
            "ERROR",
            LOG_SEVERITY_ERROR,
            LOG_DIAG_VERBOSITY_INFO,
            LOG_DIAG_VERBOSITY_TRACE,
            chain->file,
            chain->line,
            "Error context: func=`%s`",
            chain->func
        );
    }
}

static void pdf_error_ctx_free(PdfErrorCtx* chain) {
    RELEASE_ASSERT(chain);

    if (chain->message) {
        free(chain->message);
        chain->message = NULL;
    }

    if (chain->next) {
        pdf_error_ctx_free(chain->next);
        chain->next = NULL;
    }

    free(chain);
}

static char* fmt_error_message(const char* fmt, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy) + 1;
    RELEASE_ASSERT(needed > 0);
    va_end(args_copy);

    char* buffer = malloc(sizeof(char) * (unsigned long)needed);
    RELEASE_ASSERT(buffer);

    vsprintf(buffer, fmt, args);

    return buffer;
}

struct PdfError {
    PdfErrorCode code;
    PdfErrorCtx* ctx_chain;
};

PdfError* pdf_error_new(PdfErrorCode code) {
    PdfError* error = malloc(sizeof(PdfError));
    RELEASE_ASSERT(error);

    error->code = code;
    error->ctx_chain = NULL;

    return error;
}

void pdf_error_free(PdfError* error) {
    RELEASE_ASSERT(error);

    pdf_error_ctx_free(error->ctx_chain);
    free(error);
}

PdfError* pdf_error_add_context(
    PdfError* error,
    const char* func,
    const char* file,
    unsigned long line,
    const char* fmt,
    ...
) {
    RELEASE_ASSERT(error);
    RELEASE_ASSERT(func);
    RELEASE_ASSERT(file);

    if (fmt) {
        va_list args;
        va_start(args, fmt);
        char* message = fmt_error_message(fmt, args);
        va_end(args);
        error->ctx_chain =
            pdf_error_ctx_extend(error->ctx_chain, message, func, file, line);
    } else {
        error->ctx_chain =
            pdf_error_ctx_extend(error->ctx_chain, NULL, func, file, line);
    }

    return error;
}

PdfErrorCode pdf_error_code(PdfError* error) {
    RELEASE_ASSERT(error);

    return error->code;
}

void pdf_error_print(PdfError* error) {
    RELEASE_ASSERT(error);

    if (error->ctx_chain) {
        pdf_error_ctx_display(error->ctx_chain);
    }
}

void pdf_error_unwrap(PdfError* error, const char* file, unsigned long line) {
    RELEASE_ASSERT(error);
    RELEASE_ASSERT(file);

    pdf_error_print(error);

    PdfErrorCode code = error->code;
    pdf_error_free(error);

    logger_log(
        "ERROR",
        LOG_SEVERITY_PANIC,
        LOG_DIAG_VERBOSITY_INFO,
        LOG_DIAG_VERBOSITY_TRACE,
        file,
        line,
        "Error code %d occurred",
        code
    );
    exit(EXIT_FAILURE);
}

bool pdf_error_free_is_ok(PdfError* error) {
    if (error) {
        pdf_error_free(error);
        return false;
    } else {
        return true;
    }
}
