#pragma once

#include <stdbool.h>

#include "arena/arena.h"
#include "geom/mat3.h"
#include "geom/vec2.h"

typedef struct PathBuilder PathBuilder;
typedef struct PathBuilderOptions {
    bool flatten_curves;
    double quad_flatness;
    int quad_max_depth;
    double cubic_flatness;
    int cubic_max_depth;
} PathBuilderOptions;

PathBuilderOptions path_builder_options_default(void);
PathBuilderOptions path_builder_options_flattened(void);

PathBuilder* path_builder_new(Arena* arena);
PathBuilder*
path_builder_new_with_options(Arena* arena, PathBuilderOptions options);
PathBuilder* path_builder_clone(Arena* arena, const PathBuilder* to_clone);
void path_builder_set_options(PathBuilder* builder, PathBuilderOptions options);

void path_builder_new_contour(PathBuilder* builder, GeomVec2 point);
void path_builder_close_contour(PathBuilder* builder);
GeomVec2 path_builder_position(PathBuilder* builder);
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
