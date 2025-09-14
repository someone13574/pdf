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
    PdfError* next_error;
};

PdfError* pdf_error_new(PdfErrorCode code) {
    // TODO: find a way to do this without dynamically allocated memory
    PdfError* error = malloc(sizeof(PdfError));
    RELEASE_ASSERT(error);

    error->code = code;
    error->ctx_chain = NULL;
    error->next_error = NULL;

    return error;
}

void pdf_error_free(PdfError* error) {
    RELEASE_ASSERT(error);

    pdf_error_ctx_free(error->ctx_chain);
    free(error);
}

PdfError*
pdf_error_conditional_context(PdfError* error, PdfError* context_error) {
    if (!error) {
        pdf_error_free(context_error);
        return NULL;
    }

    if (!context_error) {
        return error;
    }

    RELEASE_ASSERT(error);
    RELEASE_ASSERT(context_error);

    PDF_ERROR_ADD_CONTEXT_FMT(error, "Attached context error to this error");
    PDF_ERROR_ADD_CONTEXT_FMT(
        context_error,
        "Attached this error as context to another error"
    );

    PdfError* curr_error = context_error;
    while (curr_error->next_error) {
        curr_error = curr_error->next_error;
    }

    curr_error->next_error = error;
    return context_error;
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

    PdfError* curr_error = error;

    do {
        if (fmt) {
            va_list args;
            va_start(args, fmt);
            char* message = fmt_error_message(fmt, args);
            va_end(args);
            error->ctx_chain = pdf_error_ctx_extend(
                error->ctx_chain,
                message,
                func,
                file,
                line
            );
        } else {
            error->ctx_chain =
                pdf_error_ctx_extend(error->ctx_chain, NULL, func, file, line);
        }

        curr_error = curr_error->next_error;
    } while (curr_error);

    return error;
}

PdfErrorCode pdf_error_code(const PdfError* error) {
    RELEASE_ASSERT(error);

    return error->code;
}

void pdf_error_print(const PdfError* error) {
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
    PdfError* next_error = error->next_error;

    pdf_error_free(error);

    logger_log(
        "ERROR",
        next_error ? LOG_SEVERITY_ERROR : LOG_SEVERITY_PANIC,
        LOG_DIAG_VERBOSITY_INFO,
        LOG_DIAG_VERBOSITY_TRACE,
        file,
        line,
        "Error code %d occurred",
        code
    );

    if (next_error) {
        printf("\n");
        pdf_error_unwrap(next_error, file, line);
    } else {
        exit(EXIT_FAILURE);
    }
}

bool pdf_error_free_is_ok(PdfError* error) {
    if (error) {
        pdf_error_free(error);
        return false;
    } else {
        return true;
    }
}
