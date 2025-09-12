#pragma once

#include <stdint.h>

/// 1-byte unsigned number (range: 0-255).
typedef uint8_t CffCard8;

/// 2-byte unsigned number (range: 0-65535).
typedef uint16_t CffCard16;

/// 1, 2, 3, or 4 byte offset (specified by OffSize field).
typedef uint32_t CffOffset;

/// 1-byte unsigned number specifies the size of an Offset field or fields
/// (range: 1-4).
typedef uint8_t CffOffsetSize;

/// 2-byte string identifier (range: 0-64999).
typedef uint16_t CffStringID;

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
