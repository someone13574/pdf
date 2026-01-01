#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "arena/arena.h"
#include "pdf_error/error.h"

/// A struct for storing the state required to tokenize a postscript file into
/// tokens.
typedef struct PSTokenizer PSTokenizer;

/// Create a new postscript tokenizer from a buffer
PSTokenizer*
ps_tokenizer_new(Arena* arena, const uint8_t* data, size_t data_len);

typedef enum {
    PS_TOKEN_INTEGER,
    PS_TOKEN_REAL,
    PS_TOKEN_RADIX_NUM,
    PS_TOKEN_LIT_STRING,
    PS_TOKEN_HEX_STRING,
    PS_TOKEN_B85_STRING,
    PS_TOKEN_EXE_NAME,
    PS_TOKEN_LIT_NAME,
    PS_TOKEN_IMM_NAME,
    PS_TOKEN_START_ARRAY,
    PS_TOKEN_END_ARRAY,
    PS_TOKEN_START_PROC,
    PS_TOKEN_END_PROC,
    PS_TOKEN_START_DICT,
    PS_TOKEN_END_DICT
} PSTokenType;

typedef struct {
    uint8_t* data;
    size_t len;
} PSString;

typedef struct {
    PSTokenType type;
    union {
        int32_t integer;
        double real;
        PSString string;
        char* name;
    } data;
} PSToken;

/// Get the next token in the postscript file, storing it in `token_out`. If
/// `got_token` is set to false, there were no remaining tokens to get.
PdfError*
ps_next_token(PSTokenizer* tokenizer, PSToken* token_out, bool* got_token);

/// Converts a PostscriptString into a null-terminated c-string.
char* ps_string_as_cstr(PSString string, Arena* arena);

/// Converts a hex PostscriptString into an unsigned integer.
PdfError* ps_string_as_uint(PSString string, uint64_t* out_value);
