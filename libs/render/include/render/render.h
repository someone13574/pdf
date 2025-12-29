#pragma once

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "pdf/page.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

PdfError* render_page(
    Arena* arena,
    PdfResolver* resolver,
    const PdfPage* page,
    Canvas** canvas
);
