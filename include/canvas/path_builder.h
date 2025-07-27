#pragma once

#include "arena/arena.h"
#include "canvas/canvas.h"

typedef struct PathBuilder PathBuilder;

PathBuilder* path_builder_new(Arena* arena);

void path_builder_move_to(PathBuilder* builder, double x, double y);
void path_builder_line_to(PathBuilder* builder, double x, double y);

Canvas*
path_builder_render(PathBuilder* builder, uint32_t width, uint32_t height);
