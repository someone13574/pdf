#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "log.h"
#include "pdf.h"
#include "pdf_catalog.h"
#include "pdf_doc.h"
#include "pdf_object.h"
#include "pdf_page.h"
#include "pdf_result.h"
#include "vec.h"

char* load_file_to_buffer(Arena* arena, const char* path, size_t* out_size) {
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

    PdfDocument* doc;
    PDF_REQUIRE(pdf_document_new(arena, buffer, buffer_size, &doc));

    PdfCatalog catalog;
    PDF_REQUIRE(pdf_get_catalog(doc, &catalog));

    printf("%s\n", pdf_fmt_object(arena, catalog.raw_dict));

    PdfPageTreeNode page_tree_root;
    PDF_REQUIRE(pdf_resolve_page_tree_node(&catalog.pages, doc, &page_tree_root)
    );

    for (size_t idx = 0; idx < vec_len(page_tree_root.kids); idx++) {
        PdfPageRef* page_ref = vec_get(page_tree_root.kids, idx);
        PdfPage page;
        PDF_REQUIRE(pdf_resolve_page(page_ref, doc, &page));

        printf("%s\n", pdf_fmt_object(arena, page.raw_dict));

        if (page.contents.discriminant) {
            for (size_t contents_idx = 0;
                 contents_idx < vec_len(page.contents.value.elements);
                 contents_idx++) {
                PdfStream* content =
                    vec_get(page.contents.value.elements, contents_idx);
                printf("%s\n", content->stream_bytes);
            }
        }
    }

    LOG_INFO("Finished");

    arena_free(arena);
    return 0;
}
