#pragma once

#include <stddef.h>
#include <stdint.h>

#include "err/error.h"

typedef struct {
    const uint8_t* buffer;
    size_t buffer_len;
    size_t offset;
    size_t global_offset;
} ParseCtx;

ParseCtx parse_ctx_new(const uint8_t* buffer, size_t buffer_len);

/// Create a new context with an offset and size from the start of its parent.
/// The parent's offset will move to the end of this table.
Error* parse_ctx_new_subctx(
    ParseCtx* parent,
    size_t offset,
    size_t len,
    ParseCtx* out
);

Error* parse_ctx_seek(ParseCtx* ctx, size_t offset);
Error* parse_ctx_bound_check(ParseCtx* ctx, size_t len);

Error* parse_ctx_read_i8(ParseCtx* ctx, int8_t* out);
Error* parse_ctx_read_u8(ParseCtx* ctx, uint8_t* out);

Error* parse_ctx_read_i16_le(ParseCtx* ctx, int16_t* out);
Error* parse_ctx_read_u16_le(ParseCtx* ctx, uint16_t* out);
Error* parse_ctx_read_i32_le(ParseCtx* ctx, int32_t* out);
Error* parse_ctx_read_u32_le(ParseCtx* ctx, uint32_t* out);
Error* parse_ctx_read_i64_le(ParseCtx* ctx, int64_t* out);
Error* parse_ctx_read_u64_le(ParseCtx* ctx, uint64_t* out);

Error* parse_ctx_read_i16_be(ParseCtx* ctx, int16_t* out);
Error* parse_ctx_read_u16_be(ParseCtx* ctx, uint16_t* out);
Error* parse_ctx_read_i32_be(ParseCtx* ctx, int32_t* out);
Error* parse_ctx_read_u32_be(ParseCtx* ctx, uint32_t* out);
Error* parse_ctx_read_i64_be(ParseCtx* ctx, int64_t* out);
Error* parse_ctx_read_u64_be(ParseCtx* ctx, uint64_t* out);

Error* parse_ctx_read_f32_le(ParseCtx* ctx, float* out);
Error* parse_ctx_read_f64_le(ParseCtx* ctx, double* out);
Error* parse_ctx_read_f32_be(ParseCtx* ctx, float* out);
Error* parse_ctx_read_f64_be(ParseCtx* ctx, double* out);
