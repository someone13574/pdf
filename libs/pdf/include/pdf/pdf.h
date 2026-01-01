#pragma once

#include "err/error.h"
#include "pdf/catalog.h"
#include "pdf/resolver.h"
#include "pdf/trailer.h"

void pdf_get_trailer(PdfResolver* resolver, PdfTrailer* trailer);
Error* pdf_get_catalog(PdfResolver* resolver, PdfCatalog* catalog);
