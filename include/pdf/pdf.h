#pragma once

#include "pdf/catalog.h"
#include "pdf/error.h"
#include "pdf/resolver.h"
#include "pdf/trailer.h"

PdfError* pdf_get_trailer(PdfResolver* resolver, PdfTrailer* trailer);
PdfError* pdf_get_catalog(PdfResolver* resolver, PdfCatalog* catalog);
