#pragma once

#include "pdf/catalog.h"
#include "pdf/resolver.h"
#include "pdf/result.h"
#include "pdf/trailer.h"

PdfResult pdf_get_trailer(PdfResolver* resolver, PdfTrailer* trailer);
PdfResult pdf_get_catalog(PdfResolver* resolver, PdfCatalog* catalog);
