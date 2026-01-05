#include "parse_ctx/ctx.h"

#include <string.h>

#include "err/error.h"
#include "logger/log.h"

ParseCtx parse_ctx_new(const uint8_t* buffer, size_t buffer_len) {
    RELEASE_ASSERT(buffer);
    return (ParseCtx) {.buffer = buffer,
                       .buffer_len = buffer_len,
                       .offset = 0,
                       .global_offset = 0};
}

Error* parse_ctx_new_subctx(
    ParseCtx* parent,
    size_t offset,
    size_t len,
    ParseCtx* out
) {
    RELEASE_ASSERT(parent);
    TRY(parse_ctx_seek(parent, offset));
    TRY(parse_ctx_bound_check(parent, len));

    out->buffer = parent->buffer + parent->offset;
    out->buffer_len = len;
    out->offset = 0;
    out->global_offset = parent->offset + parent->global_offset;
    parent->offset = parent->offset + len;

    return NULL;
}

Error* parse_ctx_seek(ParseCtx* ctx, size_t offset) {
    RELEASE_ASSERT(ctx);

    if (offset > ctx->buffer_len) {
        return ERROR(CTX_EOF, "Attempted to seek past EOF");
    }

    ctx->offset = offset;
    return NULL;
}

Error* parse_ctx_bound_check(ParseCtx* ctx, size_t len) {
    RELEASE_ASSERT(ctx);

    if (len > ctx->buffer_len || ctx->offset > ctx->buffer_len - len) {
        return ERROR(CTX_EOF);
    }

    return NULL;
}

Error* parse_ctx_read_i8(ParseCtx* ctx, int8_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    TRY(parse_ctx_bound_check(ctx, 1));

    int8_t x;
    memcpy(&x, ctx->buffer + ctx->offset++, 1);
    *out = x;
    return NULL;
}

Error* parse_ctx_read_u8(ParseCtx* ctx, uint8_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    TRY(parse_ctx_bound_check(ctx, 1));

    *out = ctx->buffer[ctx->offset++];
    return NULL;
}

Error* parse_ctx_read_i16_le(ParseCtx* ctx, int16_t* out) {
    RELEASE_ASSERT(out);

    uint16_t u16;
    TRY(parse_ctx_read_u16_le(ctx, &u16));
    memcpy(out, &u16, 2);
    return NULL;
}

Error* parse_ctx_read_u16_le(ParseCtx* ctx, uint16_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    TRY(parse_ctx_bound_check(ctx, 2));

    uint32_t parsed = ((uint32_t)ctx->buffer[ctx->offset + 1] << 8)
                    | (uint32_t)ctx->buffer[ctx->offset];
    *out = (uint16_t)parsed;
    ctx->offset += 2;

    return NULL;
}

Error* parse_ctx_read_i32_le(ParseCtx* ctx, int32_t* out) {
    RELEASE_ASSERT(out);

    uint32_t u32;
    TRY(parse_ctx_read_u32_le(ctx, &u32));
    memcpy(out, &u32, 4);
    return NULL;
}

Error* parse_ctx_read_u32_le(ParseCtx* ctx, uint32_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    TRY(parse_ctx_bound_check(ctx, 4));

    *out = ((uint32_t)ctx->buffer[ctx->offset + 3] << 24)
         | ((uint32_t)ctx->buffer[ctx->offset + 2] << 16)
         | ((uint32_t)ctx->buffer[ctx->offset + 1] << 8)
         | (uint32_t)ctx->buffer[ctx->offset];
    ctx->offset += 4;

    return NULL;
}

Error* parse_ctx_read_i64_le(ParseCtx* ctx, int64_t* out) {
    RELEASE_ASSERT(out);

    uint64_t u64;
    TRY(parse_ctx_read_u64_le(ctx, &u64));
    memcpy(out, &u64, 8);
    return NULL;
}

Error* parse_ctx_read_u64_le(ParseCtx* ctx, uint64_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    TRY(parse_ctx_bound_check(ctx, 8));

    *out = ((uint64_t)ctx->buffer[ctx->offset + 7] << 56)
         | ((uint64_t)ctx->buffer[ctx->offset + 6] << 48)
         | ((uint64_t)ctx->buffer[ctx->offset + 5] << 40)
         | ((uint64_t)ctx->buffer[ctx->offset + 4] << 32)
         | ((uint64_t)ctx->buffer[ctx->offset + 3] << 24)
         | ((uint64_t)ctx->buffer[ctx->offset + 2] << 16)
         | ((uint64_t)ctx->buffer[ctx->offset + 1] << 8)
         | (uint64_t)ctx->buffer[ctx->offset + 0];
    ctx->offset += 8;

    return NULL;
}

Error* parse_ctx_read_i16_be(ParseCtx* ctx, int16_t* out) {
    RELEASE_ASSERT(out);

    uint16_t u16;
    TRY(parse_ctx_read_u16_be(ctx, &u16));
    memcpy(out, &u16, 2);
    return NULL;
}

Error* parse_ctx_read_u16_be(ParseCtx* ctx, uint16_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    TRY(parse_ctx_bound_check(ctx, 2));

    uint32_t parsed = ((uint32_t)ctx->buffer[ctx->offset] << 8)
                    | (uint32_t)ctx->buffer[ctx->offset + 1];
    *out = (uint16_t)parsed;
    ctx->offset += 2;

    return NULL;
}

Error* parse_ctx_read_i32_be(ParseCtx* ctx, int32_t* out) {
    RELEASE_ASSERT(out);

    uint32_t u32;
    TRY(parse_ctx_read_u32_be(ctx, &u32));
    memcpy(out, &u32, 4);
    return NULL;
}

Error* parse_ctx_read_u32_be(ParseCtx* ctx, uint32_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    TRY(parse_ctx_bound_check(ctx, 4));

    *out = ((uint32_t)ctx->buffer[ctx->offset] << 24)
         | ((uint32_t)ctx->buffer[ctx->offset + 1] << 16)
         | ((uint32_t)ctx->buffer[ctx->offset + 2] << 8)
         | (uint32_t)ctx->buffer[ctx->offset + 3];
    ctx->offset += 4;

    return NULL;
}

Error* parse_ctx_read_i64_be(ParseCtx* ctx, int64_t* out) {
    RELEASE_ASSERT(out);

    uint64_t u64;
    TRY(parse_ctx_read_u64_be(ctx, &u64));
    memcpy(out, &u64, 8);
    return NULL;
}

Error* parse_ctx_read_u64_be(ParseCtx* ctx, uint64_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    TRY(parse_ctx_bound_check(ctx, 8));

    *out = ((uint64_t)ctx->buffer[ctx->offset] << 56)
         | ((uint64_t)ctx->buffer[ctx->offset + 1] << 48)
         | ((uint64_t)ctx->buffer[ctx->offset + 2] << 40)
         | ((uint64_t)ctx->buffer[ctx->offset + 3] << 32)
         | ((uint64_t)ctx->buffer[ctx->offset + 4] << 24)
         | ((uint64_t)ctx->buffer[ctx->offset + 5] << 16)
         | ((uint64_t)ctx->buffer[ctx->offset + 6] << 8)
         | (uint64_t)ctx->buffer[ctx->offset + 7];
    ctx->offset += 8;

    return NULL;
}

extern Error* parse_ctx_read_f32_le(ParseCtx* ctx, float* out) {
    RELEASE_ASSERT(out);

    uint32_t u32;
    TRY(parse_ctx_read_u32_le(ctx, &u32));
    memcpy(out, &u32, 4);
    return NULL;
}

extern Error* parse_ctx_read_f64_le(ParseCtx* ctx, double* out) {
    RELEASE_ASSERT(out);

    uint64_t u64;
    TRY(parse_ctx_read_u64_le(ctx, &u64));
    memcpy(out, &u64, 8);
    return NULL;
}

extern Error* parse_ctx_read_f32_be(ParseCtx* ctx, float* out) {
    RELEASE_ASSERT(out);

    uint32_t u32;
    TRY(parse_ctx_read_u32_be(ctx, &u32));
    memcpy(out, &u32, 4);
    return NULL;
}

extern Error* parse_ctx_read_f64_be(ParseCtx* ctx, double* out) {
    RELEASE_ASSERT(out);

    uint64_t u64;
    TRY(parse_ctx_read_u64_be(ctx, &u64));
    memcpy(out, &u64, 8);
    return NULL;
}
