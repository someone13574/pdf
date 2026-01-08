#include "checksum.h"

#include <stddef.h>
#include <stdint.h>

#include "err/error.h"
#include "parse_ctx/ctx.h"

Error*
sfnt_validate_checksum(ParseCtx ctx, uint32_t checksum, bool is_head_table) {
    TRY(parse_ctx_seek(&ctx, 0));

    size_t num_words = (ctx.buffer_len + 3) / 4;
    uint32_t sum = 0;
    for (size_t word_idx = 0; word_idx < num_words; word_idx++) {
        if (is_head_table && ctx.offset == 8) {
            // Ignore checksum adjustment field of head table. It will result in
            // the incorrect checksum
            ctx.offset += 4;
            continue;
        }

        if (ctx.offset + 4 <= ctx.buffer_len) {
            uint32_t word;
            REQUIRE(parse_ctx_read_u32_be(&ctx, &word));
            sum += word;
        } else {
            uint32_t word = 0;
            for (size_t byte_idx = 0; byte_idx + ctx.offset < ctx.buffer_len;
                 byte_idx++) {
                word |= (uint32_t)ctx.buffer[ctx.offset + byte_idx]
                     << (24 - 8 * byte_idx);
            }

            sum += word;
        }
    }

    if (sum != checksum) {
        return ERROR(
            SFNT_ERR_TABLE_CHECKSUM,
            "SFNT checksum mismatch %llu != %llu",
            (unsigned long long)checksum,
            (unsigned long long)sum
        );
    }

    return NULL;
}
