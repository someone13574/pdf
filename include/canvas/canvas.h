#pragma once

#include <stdint.h>

#include "arena/arena.h"

typedef struct Canvas Canvas;

Canvas* canvas_new_raster(
    Arena* arena,
    uint32_t width,
    uint32_t height,
    uint32_t rgba,
    double coordinate_scale
);

Canvas* canvas_new_scalable(Arena* arena, uint32_t width, uint32_t height);

void canvas_draw_circle(
    Canvas* canvas,
    double x,
    double y,
    double radius,
    uint32_t rgba
);

void canvas_draw_line(
    Canvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double radius,
    uint32_t rgba
);

void canvas_draw_bezier(
    Canvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double cx,
    double cy,
    double flatness,
    double radius,
    uint32_t rgba
);

/// Writes the canvas to a file. Returns `true` on success.
bool canvas_write_file(Canvas* canvas, const char* path);
