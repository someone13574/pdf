#pragma once

#include "pdf_doc.h"
#include "pdf_result.h"
#include "pdf_schema.h"

PdfSchemaTrailer* pdf_get_trailer(PdfDocument* doc, PdfResult* result);
PdfSchemaCatalog* pdf_get_catalog(PdfDocument* doc, PdfResult* result);
