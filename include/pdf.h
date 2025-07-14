#pragma once

#include "pdf_catalog.h"
#include "pdf_doc.h"
#include "pdf_result.h"
#include "pdf_trailer.h"

PdfTrailer* pdf_get_trailer(PdfDocument* doc, PdfResult* result);
PdfCatalog* pdf_get_catalog(PdfDocument* doc, PdfResult* result);
