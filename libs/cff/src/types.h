#pragma once

#include <stdint.h>

#include "parse_ctx/ctx.h"

/// 1-byte unsigned number specifies the size of an Offset field or fields
/// (range: 1-4).
typedef uint8_t CffOffsetSize;

/// 1, 2, 3, or 4 byte offset (specified by OffSize field).
typedef uint32_t CffOffset;

/// 2-byte string identifier (range: 0-64999).
typedef uint16_t CffSID;

typedef struct {
    enum { CFF_NUMBER_INT, CFF_NUMBER_REAL } type;

    union {
        int32_t integer;
        double real;
    } value;
} CffNumber;

/// Converts an integer or real to a real
double cff_num_as_real(CffNumber num);

/// The type of CffToken.
typedef enum {
    CFF_TOKEN_OPERATOR,
    CFF_TOKEN_INT_OPERAND,
    CFF_TOKEN_REAL_OPERAND
} CffTokenType;

/// A CFF operator or operand.
typedef struct {
    CffTokenType type;
    union {
        uint8_t operator;
        int32_t integer;
        double real;
    } value;
} CffToken;

#define DARRAY_NAME CffSIDArray
#define DARRAY_LOWERCASE_NAME cff_sid_array
#define DARRAY_TYPE CffSID
#include "arena/darray_decl.h"

/// Read a 1-4 byte unsigned integer from the current offset.
Error* cff_read_offset(
    ParseCtx* ctx,
    CffOffsetSize offset_size,
    CffOffset* offset_out
);

/// Read an 8-bit unsigned integer from the current offset which must have a
/// value from 1-4.
Error* cff_read_offset_size(ParseCtx* ctx, CffOffsetSize* offset_size_out);

/// Read a 2-byte string identifier which must have a value from 0 to 64999 from
/// the current offset.
Error* cff_read_sid(ParseCtx* ctx, CffSID* sid_out);

/// Read an operator or operand from the current offset.
Error* cff_read_token(ParseCtx* ctx, CffToken* token_out);

/// Get a null-terminated string of a specified length from the current offset.
Error* cff_get_str(Arena* arena, ParseCtx* ctx, size_t length, char** str_out);
