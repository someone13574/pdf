#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "pdf.h"
#include "pdf_object.h"
#include "pdf_result.h"

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

    Arena* arena = arena_new(1024);

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

    PdfObject* trailer = pdf_get_trailer(doc, &result);
    if (result != PDF_OK || !trailer) {
        LOG_ERROR("Failed to parse trailer");
        arena_free(arena);
        return EXIT_FAILURE;
    }

    char* formatted = pdf_fmt_object(arena, trailer);
    printf("Trailer:\n%s\n", formatted);

    LOG_INFO("Finished");

    arena_free(arena);
    return 0;
}
