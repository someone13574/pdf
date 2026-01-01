#include "pdf/fonts/cmap.h"

#include <stdint.h>
#include <stdio.h>

#include "arena/arena.h"
#include "arena/common.h"
#include "err/error.h"
#include "logger/log.h"

int main(void) {
    Arena* arena = arena_new(1024);

    size_t buffer_len;
    const uint8_t* buffer = load_file_to_buffer(
        arena,
        "assets/cmap-resources/Adobe-Identity-0/CMap/Identity-H",
        &buffer_len
    );

    PdfCMap* cmap = NULL;
    REQUIRE(pdf_parse_cmap(arena, buffer, buffer_len, &cmap));

    for (uint32_t codepoint = 0; codepoint <= 0xffff; codepoint++) {
        uint32_t cid;
        REQUIRE(
            pdf_cmap_get_cid(cmap, codepoint, &cid),
            "Failed to get cid for codepoint 0x%04x",
            (unsigned int)codepoint
        );

        LOG_DIAG(
            INFO,
            CMAP,
            "0x%04x = %llu",
            codepoint,
            (unsigned long long int)cid
        );
    }

    arena_free(arena);
    return 0;
}
