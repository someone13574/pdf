#pragma once

#include <stdint.h>

#include "arena/arena.h"

typedef struct Canvas Canvas;

Canvas*
canvas_new(Arena* arena, uint32_t width, uint32_t height, uint32_t rgba);

uint32_t canvas_width(Canvas* canvas);
uint32_t canvas_height(Canvas* canvas);

uint32_t canvas_get_rgba(Canvas* canvas, uint32_t x, uint32_t y);
void canvas_set_rgba(Canvas* canvas, uint32_t x, uint32_t y, uint32_t rgba);

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
    double spacing,
    double radius,
    uint32_t rgba
);

bool canvas_write_file(Canvas* canvas, const char* path);
