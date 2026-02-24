#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "canvas/path_builder.h"

typedef struct RasterCanvas RasterCanvas;

RasterCanvas* raster_canvas_new(
    Arena* arena,
    uint32_t width,
    uint32_t height,
    Rgba rgba,
    double coordinate_scale
);

double raster_canvas_raster_res(const RasterCanvas* canvas);
uint32_t raster_canvas_width(const RasterCanvas* canvas);
uint32_t raster_canvas_height(const RasterCanvas* canvas);

Rgba raster_canvas_get_rgba(const RasterCanvas* canvas, uint32_t x, uint32_t y);

void raster_canvas_set_rgba(
    RasterCanvas* canvas,
    uint32_t x,
    uint32_t y,
    Rgba rgba
);

void raster_canvas_draw_circle(
    RasterCanvas* canvas,
    double x,
    double y,
    double radius,
    Rgba rgba
);

void raster_canvas_draw_line(
    RasterCanvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double radius,
    Rgba rgba
);

void raster_canvas_draw_arrow(
    RasterCanvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double radius,
    double tip_radius,
    Rgba rgba
);

void raster_canvas_draw_bezier(
    RasterCanvas* canvas,
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

void raster_canvas_draw_path(
    RasterCanvas* canvas,
    const PathBuilder* path,
    CanvasBrush brush
);

void raster_canvas_push_clip_path(
    RasterCanvas* canvas,
    const PathBuilder* path,
    bool even_odd_rule
);

void raster_canvas_pop_clip_paths(RasterCanvas* canvas, size_t count);

void raster_canvas_draw_pixel(
    RasterCanvas* canvas,
    GeomVec2 position,
    Rgba rgba
);

bool raster_canvas_write_file(RasterCanvas* canvas, const char* path);
