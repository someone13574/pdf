#pragma once

#include "arena/arena.h"
#include "canvas/canvas.h"

typedef struct DcelBuilder DcelBuilder;

DcelBuilder* dcel_builder_new(Arena* arena);

void dcel_builder_new_contour(DcelBuilder* builder, double x, double y);
void dcel_builder_line_to(DcelBuilder* builder, double x, double y);
void dcel_builder_end_contour(DcelBuilder* builder);

Canvas* dcel_builder_render(
    const DcelBuilder* builder,
    uint32_t width,
    uint32_t height
);
