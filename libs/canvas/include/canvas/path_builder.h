#pragma once

#include "arena/arena.h"
#include "geom/mat3.h"
#include "geom/vec2.h"

typedef struct PathBuilder PathBuilder;

PathBuilder* path_builder_new(Arena* arena);
PathBuilder* path_builder_clone(Arena* arena, const PathBuilder* to_clone);

void path_builder_new_contour(PathBuilder* builder, GeomVec2 point);
void path_builder_close_contour(PathBuilder* builder);
void path_builder_line_to(PathBuilder* builder, GeomVec2 point);
void path_builder_quad_bezier_to(
    PathBuilder* builder,
    GeomVec2 end,
    GeomVec2 control
);
void path_builder_cubic_bezier_to(
    PathBuilder* builder,
    GeomVec2 end,
    GeomVec2 control_a,
    GeomVec2 control_b
);
void path_builder_apply_transform(PathBuilder* path, GeomMat3 transform);
