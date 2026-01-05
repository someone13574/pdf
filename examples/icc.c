#include "color/icc.h"

#include <stdint.h>

#include "arena/arena.h"
#include "arena/common.h"
#include "color/icc_tags.h"
#include "err/error.h"
#include "parse_ctx/ctx.h"

int main(void) {
    Arena* arena = arena_new(128);

    size_t buffer_len;
    const uint8_t* buffer = load_file_to_buffer(
        arena,
        "test-files/USWebCoatedSWOP.icc",
        &buffer_len
    );

    ParseCtx ctx = parse_ctx_new(buffer, buffer_len);
    ICCProfileHeader header;
    REQUIRE(icc_parse_header(&ctx, &header));

    ICCTagTable table;
    REQUIRE(icc_tag_table_new(&ctx, &table));

    ParseCtx copyright_ctx;
    REQUIRE(icc_tag_table_lookup(
        table,
        icc_tag_signature(ICC_TAG_COPYRIGHT),
        &copyright_ctx
    ));

    arena_free(arena);
    return 0;
}
