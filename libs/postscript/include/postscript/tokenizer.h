#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "arena/arena.h"
#include "pdf_error/error.h"

/// A struct for storing the state required to tokenize a postscript file into
/// tokens.
typedef struct PostscriptTokenizer PostscriptTokenizer;

/// Create a new postscript tokenizer from a buffer
PostscriptTokenizer*
postscript_tokenizer_new(Arena* arena, const uint8_t* data, size_t data_len);

typedef enum {
    POSTSCRIPT_TOKEN_INTEGER,
    POSTSCRIPT_TOKEN_REAL,
    POSTSCRIPT_TOKEN_RADIX_NUM,
    POSTSCRIPT_TOKEN_LIT_STRING,
    POSTSCRIPT_TOKEN_HEX_STRING,
    POSTSCRIPT_TOKEN_B85_STRING,
    POSTSCRIPT_TOKEN_EXE_NAME,
    POSTSCRIPT_TOKEN_LIT_NAME,
    POSTSCRIPT_TOKEN_IMM_NAME,
    POSTSCRIPT_TOKEN_START_ARRAY,
    POSTSCRIPT_TOKEN_END_ARRAY,
    POSTSCRIPT_TOKEN_START_PROC,
    POSTSCRIPT_TOKEN_END_PROC,
    POSTSCRIPT_TOKEN_START_DICT,
    POSTSCRIPT_TOKEN_END_DICT
} PostscriptTokenType;

typedef struct {
    uint8_t* data;
    size_t len;
} PostscriptString;

typedef struct {
    PostscriptTokenType type;
    union {
        int32_t integer;
        double real;
        PostscriptString string;
        char* name;
    } data;
} PostscriptToken;

/// Get the next token in the postscript file, storing it in `token_out`. If
/// `got_token` is set to false, there were no remaining tokens to get.
PdfError* postscript_next_token(
    PostscriptTokenizer* tokenizer,
    PostscriptToken* token_out,
    bool* got_token
);

/// Converts a PostscriptString into a null-terminated c-string.
char* postscript_string_as_cstr(PostscriptString string, Arena* arena);

/// Converts a hex PostscriptString into an unsigned integer.
PdfError*
postscript_string_as_uint(PostscriptString string, uint64_t* out_value);
