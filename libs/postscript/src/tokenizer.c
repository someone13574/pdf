#include "postscript/tokenizer.h"

#include "arena/arena.h"
#include "logger/log.h"

struct PostscriptTokenizer {
    const char* data;
    size_t data_len;
    size_t offset;
};

PostscriptTokenizer*
postscript_tokenizer_new(Arena* arena, const char* data, size_t data_len) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(data);

    PostscriptTokenizer* tokenizer =
        arena_alloc(arena, sizeof(PostscriptTokenizer));
    tokenizer->data = data;
    tokenizer->data_len = data_len;
    tokenizer->offset = 0;

    return tokenizer;
}

PdfError* postscript_next_token(
    PostscriptTokenizer* tokenizer,
    PostscriptToken* token_out
) {
    RELEASE_ASSERT(tokenizer);
    RELEASE_ASSERT(token_out);

    bool in_comment = false;
    bool prev_carriage_return =
        false; // Track whether the previous char was a CR so we can treat both
               // \r and \r\n as a single newline.

    while (tokenizer->offset < tokenizer->data_len) {
        char c = tokenizer->data[tokenizer->offset++];

        // Consume comments
        if (in_comment) {
            if (prev_carriage_return) {
                in_comment = false;
                prev_carriage_return = false;
                continue;
            }

            if (c == '\n') {
                in_comment = false;
                continue;
            } else if (c == '\r') {
                prev_carriage_return = true;
                continue;
            }
        }
    }

    return NULL;
}
