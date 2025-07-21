#pragma once

#include "pdf_catalog.h"
#include "pdf_resolver.h"
#include "pdf_result.h"
#include "pdf_trailer.h"

PdfResult pdf_get_trailer(PdfResolver* resolver, PdfTrailer* trailer);
PdfResult pdf_get_catalog(PdfResolver* resolver, PdfCatalog* catalog);
