#pragma once

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "pdf/page.h"

Canvas* render_page(Arena* arena, PdfPage* page);
