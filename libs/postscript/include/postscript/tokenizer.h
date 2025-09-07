#pragma once

#include <stdlib.h>

#include "arena/arena.h"
#include "pdf_error/error.h"

/// A struct for storing the state required to tokenize a postscript file into
/// tokens.
typedef struct PostscriptTokenizer PostscriptTokenizer;

/// Create a new postscript tokenizer from a buffer
PostscriptTokenizer*
postscript_tokenizer_new(Arena* arena, const char* data, size_t data_len);

typedef enum {
    POSTSCRIPT_TOKEN_INTEGER,
    POSTSCRIPT_TOKEN_REAL,
    POSTSCRIPT_TOKEN_RADIX_NUM,
    POSTSCRIPT_TOKEN_LIT_STRING,
    POSTSCRIPT_TOKEN_HEX_STRING,
    POSTSCRIPT_TOKEN_B85_STRING,
    POSTSCRIPT_TOKEN_NAME,
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
    PostscriptTokenType type;
} PostscriptToken;

/// Get the next token in the postscript file, storing it in `token_out`.
PdfError* postscript_next_token(
    PostscriptTokenizer* tokenizer,
    PostscriptToken* token_out
);
