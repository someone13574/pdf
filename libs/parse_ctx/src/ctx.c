#include "parse_ctx/ctx.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "arena/common.h"
#include "err/error.h"
#include "logger/log.h"

ParseCtx parse_ctx_new(const uint8_t* buffer, size_t buffer_len) {
    RELEASE_ASSERT(buffer);
    return (ParseCtx) {.buffer = buffer,
                       .buffer_len = buffer_len,
                       .offset = 0,
                       .global_offset = 0};
}

ParseCtx parse_ctx_from_file(Arena* arena, const char* path) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(path);

    size_t len;
    const uint8_t* buffer = load_file_to_buffer(arena, path, &len);
    RELEASE_ASSERT(buffer, "Invalid path");

    return parse_ctx_new(buffer, len);
}

Error* parse_ctx_new_subctx(ParseCtx* parent, size_t len, ParseCtx* out) {
    RELEASE_ASSERT(parent);
    TRY(parse_ctx_bound_check(parent, len));

    out->buffer = parent->buffer + parent->offset;
    out->buffer_len = len;
    out->offset = 0;
    out->global_offset = parent->offset + parent->global_offset;
    parent->offset = parent->offset + len;

    return NULL;
}

Error* parse_ctx_align(ParseCtx* ctx, size_t align, bool require_zeros) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(align > 0);

    size_t next_global_offset =
        (ctx->offset + ctx->global_offset + align - 1) / align * align;
    size_t next_local_offset = next_global_offset - ctx->global_offset;

    if (require_zeros) {
        DEBUG_ASSERT(next_local_offset >= ctx->offset);
        TRY(parse_ctx_bound_check(ctx, next_local_offset - ctx->offset));

        for (; ctx->offset < next_local_offset; ctx->offset++) {
            if (ctx->buffer[ctx->offset] != 0) {
                return ERROR(CTX_NO_PAD);
            }
        }
    } else {
        TRY(parse_ctx_seek(ctx, next_local_offset));
    }

    return NULL;
}
