#include <stdio.h>
#include <stdlib.h>

#include "pdf.h"

char* load_file_to_buffer(const char* path, size_t* out_size) {
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

    char* buffer = malloc((size_t)len);
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

    size_t buffer_size;
    char* buffer = load_file_to_buffer("test-files/test.pdf", &buffer_size);

    PdfDocument* doc = pdf_document_new(buffer, buffer_size);

    pdf_document_free(doc);
    doc = NULL;

    free(buffer);
    buffer = NULL;

    return 0;
}
