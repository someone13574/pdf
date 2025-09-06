#pragma once

#include "arena/arena.h"

typedef struct PathBuilder PathBuilder;

PathBuilder* path_builder_new(Arena* arena);

void path_builder_new_contour(PathBuilder* builder, double x, double y);
void path_builder_line_to(PathBuilder* builder, double x, double y);
void path_builder_bezier_to(
    PathBuilder* builder,
    double x,
    double y,
    double cx,
    double cy
);
