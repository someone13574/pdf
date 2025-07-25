#pragma once

#include "pdf/catalog.h"
#include "pdf/resolver.h"
#include "pdf/trailer.h"
#include "pdf_error/error.h"

PdfError* pdf_get_trailer(PdfResolver* resolver, PdfTrailer* trailer);
PdfError* pdf_get_catalog(PdfResolver* resolver, PdfCatalog* catalog);
