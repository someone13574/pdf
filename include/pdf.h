#pragma once

#include <stdlib.h>

typedef struct PdfDocument PdfDocument;

PdfDocument* pdf_document_new(char* buffer, size_t buffer_size);
void pdf_document_free(PdfDocument* doc);
