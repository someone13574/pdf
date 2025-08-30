#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "canvas/path_builder.h"

typedef struct RasterCanvas RasterCanvas;

RasterCanvas* raster_canvas_new(
    Arena* arena,
    uint32_t width,
    uint32_t height,
    uint32_t rgba,
    double coordinate_scale
);

uint32_t raster_canvas_width(RasterCanvas* canvas);
uint32_t raster_canvas_height(RasterCanvas* canvas);

uint32_t raster_canvas_get_rgba(RasterCanvas* canvas, uint32_t x, uint32_t y);

void raster_canvas_set_rgba(
    RasterCanvas* canvas,
    uint32_t x,
    uint32_t y,
    uint32_t rgba
);

void raster_canvas_draw_circle(
    RasterCanvas* canvas,
    double x,
    double y,
    double radius,
    uint32_t rgba
);

void raster_canvas_draw_line(
    RasterCanvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double radius,
    uint32_t rgba
);

void raster_canvas_draw_arrow(
    RasterCanvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double radius,
    double tip_radius,
    uint32_t rgba
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
    uint32_t rgba
);

void raster_canvas_draw_path(RasterCanvas* canvas, PathBuilder* path);

bool raster_canvas_write_file(RasterCanvas* canvas, const char* path);
