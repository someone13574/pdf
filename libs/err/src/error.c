#include "err/error.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "logger/log.h"

typedef struct ErrorCtx {
    struct ErrorCtx* next;

    char* message;
    const char* func;
    const char* file;
    unsigned long line;
} ErrorCtx;

static ErrorCtx* error_ctx_extend(
    ErrorCtx* chain,
    char* message,
    const char* func,
    const char* file,
    unsigned long line
) {
    RELEASE_ASSERT(func);
    RELEASE_ASSERT(file);

    ErrorCtx* ctx = malloc(sizeof(ErrorCtx));
    RELEASE_ASSERT(ctx);

    ctx->next = chain;
    ctx->message = message;
    ctx->func = func;
    ctx->file = file;
    ctx->line = line;

    return ctx;
}

static void error_ctx_display(ErrorCtx* chain) {
    RELEASE_ASSERT(chain);

    if (chain->next) {
        error_ctx_display(chain->next);
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

static void error_ctx_free(ErrorCtx* chain) {
    RELEASE_ASSERT(chain);

    if (chain->message) {
        free(chain->message);
        chain->message = NULL;
    }

    if (chain->next) {
        error_ctx_free(chain->next);
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

struct Error {
    ErrorCode code;
    ErrorCtx* ctx_chain;
    Error* next_error;
};

Error* error_new(ErrorCode code) {
    // TODO: find a way to do this without dynamically allocated memory
    Error* error = malloc(sizeof(Error));
    RELEASE_ASSERT(error);

    error->code = code;
    error->ctx_chain = NULL;
    error->next_error = NULL;

    return error;
}

void error_free(Error* error) {
    RELEASE_ASSERT(error);

    error_ctx_free(error->ctx_chain);
    free(error);
}

Error* error_conditional_context(Error* error, Error* context_error) {
    if (!error) {
        error_free(context_error);
        return NULL;
    }

    if (!context_error) {
        return error;
    }

    RELEASE_ASSERT(error);
    RELEASE_ASSERT(context_error);

    ERROR_ADD_CONTEXT_FMT(error, "Attached context error to this error");
    ERROR_ADD_CONTEXT_FMT(
        context_error,
        "Attached this error as context to another error"
    );

    Error* curr_error = context_error;
    while (curr_error->next_error) {
        curr_error = curr_error->next_error;
    }

    curr_error->next_error = error;
    return context_error;
}

Error* error_add_context(
    Error* error,
    const char* func,
    const char* file,
    unsigned long line,
    const char* fmt,
    ...
) {
    RELEASE_ASSERT(error);
    RELEASE_ASSERT(func);
    RELEASE_ASSERT(file);

    Error* curr_error = error;

    do {
        if (fmt) {
            va_list args;
            va_start(args, fmt);
            char* message = fmt_error_message(fmt, args);
            va_end(args);
            error->ctx_chain =
                error_ctx_extend(error->ctx_chain, message, func, file, line);
        } else {
            error->ctx_chain =
                error_ctx_extend(error->ctx_chain, NULL, func, file, line);
        }

        curr_error = curr_error->next_error;
    } while (curr_error);

    return error;
}

ErrorCode error_code(const Error* error) {
    RELEASE_ASSERT(error);

    return error->code;
}

void error_print(const Error* error) {
    RELEASE_ASSERT(error);

    if (error->ctx_chain) {
        error_ctx_display(error->ctx_chain);
    }
}

void error_unwrap(Error* error, const char* file, unsigned long line) {
    RELEASE_ASSERT(error);
    RELEASE_ASSERT(file);

    error_print(error);

    ErrorCode code = error->code;
    Error* next_error = error->next_error;

    error_free(error);

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
        error_unwrap(next_error, file, line);
    } else {
        exit(EXIT_FAILURE);
    }
}

bool error_free_is_ok(Error* error) {
    if (error) {
        error_free(error);
        return false;
    } else {
        return true;
    }
}
