#include "pdf/pdf.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "arena/arena.h"
#include "arena/common.h"
#include "canvas/canvas.h"
#include "err/error.h"
#include "logger/log.h"
#include "pdf/catalog.h"
#include "pdf/page.h"
#include "pdf/resolver.h"
#include "render/render.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Arena* arena = arena_new(8192);

    size_t buffer_size;
    uint8_t* buffer =
        load_file_to_buffer(arena, "test-files/wiki.pdf", &buffer_size);
    RELEASE_ASSERT(buffer);

    PdfResolver* resolver;
    REQUIRE(pdf_resolver_new(arena, buffer, buffer_size, &resolver));

    PdfCatalog catalog;
    REQUIRE(pdf_get_catalog(resolver, &catalog));

    PdfPageIter* page_iter = NULL;
    REQUIRE(pdf_page_iter_new(resolver, catalog.pages, &page_iter));

    bool iter_done = false;
    PdfPage page;
    while (true) {
        REQUIRE(pdf_page_iter_next(page_iter, &page, &iter_done));
        if (iter_done) {
            break;
        }

        Canvas* canvas = NULL;
        REQUIRE(render_page(arena, resolver, &page, &canvas));
        canvas_write_file(canvas, "test.svg");
    };

    LOG_DIAG(INFO, EXAMPLE, "Finished");

    arena_free(arena);
    return 0;
}
