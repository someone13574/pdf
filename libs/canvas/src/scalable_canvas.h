#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "canvas/path_builder.h"
#include "geom/vec2.h"

typedef struct ScalableCanvas ScalableCanvas;

ScalableCanvas* scalable_canvas_new(
    Arena* arena,
    uint32_t width,
    uint32_t height,
    Rgba rgba,
    double raster_res
);

double scalable_canvas_raster_res(ScalableCanvas* canvas);

void scalable_canvas_draw_circle(
    ScalableCanvas* canvas,
    double x,
    double y,
    double radius,
    Rgba rgba
);

void scalable_canvas_draw_line(
    ScalableCanvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double radius,
    Rgba rgba
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
    Rgba rgba
);

void scalable_canvas_draw_path(
    ScalableCanvas* canvas,
    const PathBuilder* path,
    CanvasBrush brush
);

void scalable_canvas_push_clip_path(
    ScalableCanvas* canvas,
    const PathBuilder* path,
    bool even_odd_rule
);

void scalable_canvas_pop_clip_paths(ScalableCanvas* canvas, size_t count);

void scalable_canvas_draw_pixel(
    ScalableCanvas* canvas,
    GeomVec2 position,
    Rgba rgba
);

bool scalable_canvas_write_file(ScalableCanvas* canvas, const char* path);
