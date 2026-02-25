#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena/arena.h"
#include "canvas/path_builder.h"
#include "color/rgb.h"

typedef enum CanvasLineCap {
    CANVAS_LINECAP_BUTT,
    CANVAS_LINECAP_ROUND,
    CANVAS_LINECAP_SQUARE
} CanvasLineCap;

typedef enum CanvasLineJoin {
    CANVAS_LINEJOIN_MITER,
    CANVAS_LINEJOIN_ROUND,
    CANVAS_LINEJOIN_BEVEL
} CanvasLineJoin;

typedef struct CanvasBrush {
    bool enable_fill;
    bool even_odd_fill;
    bool enable_stroke;

    Rgba fill_rgba;
    Rgba stroke_rgba;

    double stroke_width;
    CanvasLineCap line_cap;
    CanvasLineJoin line_join;
    double miter_limit;
} CanvasBrush;

typedef struct Canvas Canvas;

Canvas* canvas_new_raster(
    Arena* arena,
    uint32_t width,
    uint32_t height,
    Rgba rgba,
    double coordinate_scale
);

Canvas* canvas_new_scalable(
    Arena* arena,
    uint32_t width,
    uint32_t height,
    Rgba rgba,
    double raster_res
);

bool canvas_is_raster(Canvas* canvas);
double canvas_raster_res(Canvas* canvas);

void canvas_draw_circle(
    Canvas* canvas,
    double x,
    double y,
    double radius,
    Rgba rgba
);

void canvas_draw_line(
    Canvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double radius,
    Rgba rgba
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
    Rgba rgba
);

void canvas_draw_path(
    Canvas* canvas,
    const PathBuilder* path,
    CanvasBrush brush
);

void canvas_push_clip_path(
    Canvas* canvas,
    const PathBuilder* path,
    bool even_odd_rule
);

void canvas_pop_clip_paths(Canvas* canvas, size_t count);

void canvas_draw_pixel(Canvas* canvas, GeomVec2 position, Rgba rgba);

/// Writes the canvas to a file. Returns `true` on success.
bool canvas_write_file(Canvas* canvas, const char* path);
