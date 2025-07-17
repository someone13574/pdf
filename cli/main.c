#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "pdf.h"
#include "pdf_catalog.h"
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
        free(buffer);
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

    PdfResult result = PDF_OK;
    PdfDocument* doc = pdf_document_new(arena, buffer, buffer_size, &result);
    if (result != PDF_OK || !doc) {
        LOG_ERROR("Failed to load document");
        arena_free(arena);
        return EXIT_FAILURE;
    }

    PdfCatalog* catalog = pdf_get_catalog(doc, &result);
    if (result != PDF_OK || !catalog) {
        LOG_ERROR("Failed to get catalog with code %d", result);
        arena_free(arena);
        return EXIT_FAILURE;
    }

    printf("%s\n", pdf_fmt_object(arena, catalog->raw_dict));

    PdfPageTreeNode* page_tree_root =
        pdf_resolve_page_tree_node(&catalog->pages, doc, &result);
    if (result != PDF_OK || !page_tree_root) {
        LOG_ERROR("Failed to resolve page tree root with code %d", result);
        arena_free(arena);
        return EXIT_FAILURE;
    }

    for (size_t idx = 0; idx < vec_len(page_tree_root->kids); idx++) {
        PdfPageRef* page_ref = vec_get(page_tree_root->kids, idx);
        PdfPage* page = pdf_resolve_page(page_ref, doc, &result);
        if (result != PDF_OK || !page) {
            LOG_ERROR("Failed to resolve page with code %d", result);
            arena_free(arena);
            return EXIT_FAILURE;
        }

        printf("%s\n", pdf_fmt_object(arena, page->raw_dict));

        if (page->contents.discriminant) {
            for (size_t contents_idx = 0;
                 contents_idx < vec_len(page->contents.value.elements);
                 contents_idx++) {
                PdfStream* content =
                    vec_get(page->contents.value.elements, contents_idx);
                printf("%s\n", content->stream_bytes);
            }
        }
    }

    LOG_INFO("Finished");

    arena_free(arena);
    return 0;
}
