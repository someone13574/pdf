#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "pdf.h"
#include "pdf_object.h"
#include "pdf_result.h"
#include "pdf_schema.h"

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

    PdfSchemaCatalog* catalog = pdf_get_catalog(doc, &result);
    if (result != PDF_OK || !catalog) {
        LOG_ERROR("Failed to get catalog with code %d", result);
        arena_free(arena);
        return EXIT_FAILURE;
    }

    PdfSchemaPageTreeNode* page_tree_node =
        PDF_REF_GET(catalog->pages, doc, &result);
    if (result != PDF_OK || !page_tree_node) {
        LOG_ERROR("Failed to get root page tree node with code %d", result);
        arena_free(arena);
        return EXIT_FAILURE;
    }
    printf("Root:\n%s\n", pdf_fmt_object(arena, page_tree_node->dict));

    LOG_INFO("Finished");

    arena_free(arena);
    return 0;
}
