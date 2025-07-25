#include <stdio.h>
#include <stdlib.h>

#include "arena/arena.h"
#include "log.h"
#include "pdf/catalog.h"
#include "pdf/content_stream.h"
#include "pdf/error.h"
#include "pdf/object.h"
#include "pdf/page.h"
#include "pdf/pdf.h"
#include "pdf/resolver.h"

static char*
load_file_to_buffer(Arena* arena, const char* path, size_t* out_size) {
    FILE* file = fopen(path, "rb");
    *out_size = 0;
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long len = ftell(file);
    if (len < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char* buffer = arena_alloc(arena, (size_t)len);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)len, file) != (size_t)len) {
        fclose(file);
        return NULL;
    }
    fclose(file);

    *out_size = (size_t)len;
    return buffer;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Arena* arena = arena_new(4096);

    size_t buffer_size;
    char* buffer =
        load_file_to_buffer(arena, "test-files/test.pdf", &buffer_size);

    PdfResolver* resolver;
    PDF_REQUIRE(pdf_resolver_new(arena, buffer, buffer_size, &resolver));

    PdfCatalog catalog;
    PDF_REQUIRE(pdf_get_catalog(resolver, &catalog));

    printf("%s\n", pdf_fmt_object(arena, catalog.raw_dict));

    PdfPageTreeNode page_tree_root;
    PDF_REQUIRE(
        pdf_resolve_page_tree_node(&catalog.pages, resolver, &page_tree_root)
    );

    for (size_t idx = 0; idx < pdf_void_vec_len(page_tree_root.kids); idx++) {
        void* page_ptr;
        RELEASE_ASSERT(pdf_void_vec_get(page_tree_root.kids, idx, &page_ptr));
        PdfPage page;
        PDF_REQUIRE(pdf_resolve_page(page_ptr, resolver, &page));

        printf("%s\n", pdf_fmt_object(arena, page.raw_dict));

        if (page.contents.discriminant) {
            for (size_t contents_idx = 0;
                 contents_idx < pdf_void_vec_len(page.contents.value.elements);
                 contents_idx++) {
                void* content_ptr;
                RELEASE_ASSERT(pdf_void_vec_get(
                    page.contents.value.elements,
                    contents_idx,
                    &content_ptr
                ));
                PdfStream* content = content_ptr;

                printf("%s\n", content->stream_bytes);

                PdfContentStream stream;
                PDF_REQUIRE(
                    pdf_deserialize_content_stream(content, arena, &stream)
                );
            }
        }
    }

    LOG_DIAG(INFO, EXAMPLE, "Finished");

    arena_free(arena);
    return 0;
}
