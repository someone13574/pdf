#pragma once

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "err/error.h"
#include "pdf/page.h"
#include "pdf/resolver.h"

Error* render_page(
    Arena* arena,
    PdfResolver* resolver,
    const PdfPage* page,
    Canvas** canvas
);
