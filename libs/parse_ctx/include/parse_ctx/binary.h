#pragma once

#include "err/error.h"
#include "parse_ctx/ctx.h"

// Byte read functions (increments current offset)
static inline Error* parse_ctx_read_i8(ParseCtx* ctx, int8_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    TRY(parse_ctx_bound_check(ctx, 1));

    int8_t x;
    memcpy(&x, ctx->buffer + ctx->offset++, 1);
    *out = x;
    return NULL;
}

static inline Error* parse_ctx_read_u8(ParseCtx* ctx, uint8_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    TRY(parse_ctx_bound_check(ctx, 1));

    *out = ctx->buffer[ctx->offset++];
    return NULL;
}

// Little-endian read functions (increments current offset)
static inline Error* parse_ctx_read_u16_le(ParseCtx* ctx, uint16_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    TRY(parse_ctx_bound_check(ctx, 2));

    uint32_t parsed = ((uint32_t)ctx->buffer[ctx->offset + 1] << 8)
                    | (uint32_t)ctx->buffer[ctx->offset];
    *out = (uint16_t)parsed;
    ctx->offset += 2;

    return NULL;
}

static inline Error* parse_ctx_read_i16_le(ParseCtx* ctx, int16_t* out) {
    RELEASE_ASSERT(out);

    uint16_t u16;
    TRY(parse_ctx_read_u16_le(ctx, &u16));
    memcpy(out, &u16, 2);
    return NULL;
}

static inline Error* parse_ctx_read_u32_le(ParseCtx* ctx, uint32_t* out) {
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

static inline Error* parse_ctx_read_i32_le(ParseCtx* ctx, int32_t* out) {
    RELEASE_ASSERT(out);

    uint32_t u32;
    TRY(parse_ctx_read_u32_le(ctx, &u32));
    memcpy(out, &u32, 4);
    return NULL;
}

static inline Error* parse_ctx_read_u64_le(ParseCtx* ctx, uint64_t* out) {
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

static inline Error* parse_ctx_read_i64_le(ParseCtx* ctx, int64_t* out) {
    RELEASE_ASSERT(out);

    uint64_t u64;
    TRY(parse_ctx_read_u64_le(ctx, &u64));
    memcpy(out, &u64, 8);
    return NULL;
}

// Big-endian read functions (increments current offset)
static inline Error* parse_ctx_read_u16_be(ParseCtx* ctx, uint16_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    TRY(parse_ctx_bound_check(ctx, 2));

    uint32_t parsed = ((uint32_t)ctx->buffer[ctx->offset] << 8)
                    | (uint32_t)ctx->buffer[ctx->offset + 1];
    *out = (uint16_t)parsed;
    ctx->offset += 2;

    return NULL;
}

static inline Error* parse_ctx_read_i16_be(ParseCtx* ctx, int16_t* out) {
    RELEASE_ASSERT(out);

    uint16_t u16;
    TRY(parse_ctx_read_u16_be(ctx, &u16));
    memcpy(out, &u16, 2);
    return NULL;
}

static inline Error* parse_ctx_read_u32_be(ParseCtx* ctx, uint32_t* out) {
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

static inline Error* parse_ctx_read_i32_be(ParseCtx* ctx, int32_t* out) {
    RELEASE_ASSERT(out);

    uint32_t u32;
    TRY(parse_ctx_read_u32_be(ctx, &u32));
    memcpy(out, &u32, 4);
    return NULL;
}

static inline Error* parse_ctx_read_u64_be(ParseCtx* ctx, uint64_t* out) {
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

static inline Error* parse_ctx_read_i64_be(ParseCtx* ctx, int64_t* out) {
    RELEASE_ASSERT(out);

    uint64_t u64;
    TRY(parse_ctx_read_u64_be(ctx, &u64));
    memcpy(out, &u64, 8);
    return NULL;
}

// Floating-point read functions (increments current offset)
static inline Error* parse_ctx_read_f32_le(ParseCtx* ctx, float* out) {
    RELEASE_ASSERT(out);

    uint32_t u32;
    TRY(parse_ctx_read_u32_le(ctx, &u32));
    memcpy(out, &u32, 4);
    return NULL;
}

static inline Error* parse_ctx_read_f64_le(ParseCtx* ctx, double* out) {
    RELEASE_ASSERT(out);

    uint64_t u64;
    TRY(parse_ctx_read_u64_le(ctx, &u64));
    memcpy(out, &u64, 8);
    return NULL;
}

static inline Error* parse_ctx_read_f32_be(ParseCtx* ctx, float* out) {
    RELEASE_ASSERT(out);

    uint32_t u32;
    TRY(parse_ctx_read_u32_be(ctx, &u32));
    memcpy(out, &u32, 4);
    return NULL;
}

static inline Error* parse_ctx_read_f64_be(ParseCtx* ctx, double* out) {
    RELEASE_ASSERT(out);

    uint64_t u64;
    TRY(parse_ctx_read_u64_be(ctx, &u64));
    memcpy(out, &u64, 8);
    return NULL;
}

// Index a ctx without modifying the offset
static inline Error* parse_ctx_get_u8(ParseCtx ctx, size_t idx, uint8_t* out) {
    RELEASE_ASSERT(out);

    ctx.offset = idx;
    TRY(parse_ctx_read_u8(&ctx, out));

    return NULL;
}

static inline Error*
parse_ctx_get_u16_be(ParseCtx ctx, size_t idx, uint16_t* out) {
    RELEASE_ASSERT(out);

    ctx.offset = idx * 2;
    TRY(parse_ctx_read_u16_be(&ctx, out));

    return NULL;
}
