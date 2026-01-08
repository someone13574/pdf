#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena/arena.h"
#include "err/error.h"
#include "logger/log.h"

typedef struct {
    const uint8_t* buffer;
    size_t buffer_len;
    size_t offset;
    size_t global_offset;
} ParseCtx;

ParseCtx parse_ctx_new(const uint8_t* buffer, size_t buffer_len);
ParseCtx parse_ctx_from_file(Arena* arena, const char* path);

/// Create a new context with an offset and size from the start of its parent.
/// The parent's offset will move to the end of this table.
Error* parse_ctx_new_subctx(ParseCtx* parent, size_t len, ParseCtx* out);

/// Seeks a position within the subctx
static inline Error* parse_ctx_seek(ParseCtx* ctx, size_t offset) {
    RELEASE_ASSERT(ctx);

    if (offset > ctx->buffer_len) {
        return ERROR(CTX_EOF, "Attempted to seek past EOF");
    }

    ctx->offset = offset;
    return NULL;
}

/// Aligns the current offset to the next multiple of `align` within the *global
/// offset*. If `require_zeros` is true, all skipped bytes must be zero.
Error* parse_ctx_align(ParseCtx* ctx, size_t align, bool require_zeros);

/// Produces an error a `len` byte read would go out of bounds.
static inline Error* parse_ctx_bound_check(ParseCtx* ctx, size_t len) {
    RELEASE_ASSERT(ctx);

    if (len > ctx->buffer_len || ctx->offset > ctx->buffer_len - len) {
        return ERROR(
            CTX_EOF,
            "Attempted to read %zu bytes starting at offset %zu in buffer size %zu",
            len,
            ctx->offset,
            ctx->buffer_len
        );
    }

    return NULL;
}
