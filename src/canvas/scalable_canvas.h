#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "canvas/path_builder.h"

typedef struct ScalableCanvas ScalableCanvas;

ScalableCanvas*
scalable_canvas_new(Arena* arena, uint32_t width, uint32_t height);

void scalable_canvas_draw_circle(
    ScalableCanvas* canvas,
    double x,
    double y,
    double radius,
    uint32_t rgba
);

void scalable_canvas_draw_line(
    ScalableCanvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double radius,
    uint32_t rgba
);

void scalable_canvas_draw_bezier(
    ScalableCanvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double cx,
    double cy,
    double radius,
    uint32_t rgba
);

void scalable_canvas_draw_path(ScalableCanvas* canvas, PathBuilder* path);

bool scalable_canvas_write_file(ScalableCanvas* canvas, const char* path);
