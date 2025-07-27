#include "canvas/path_builder.h"

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "logger/log.h"
#include "tessellate.h"

struct PathBuilder {
    Arena* arena;

    double x;
    double y;
    TessPoly poly;
};

PathBuilder* path_builder_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    PathBuilder* builder = arena_alloc(arena, sizeof(PathBuilder));
    builder->arena = arena;
    builder->x = 0.0;
    builder->y = 0.0;
    tess_poly_new(arena, &builder->poly);

    return builder;
}

void path_builder_move_to(PathBuilder* builder, double x, double y) {
    RELEASE_ASSERT(builder);

    builder->x = x;
    builder->y = y;
    tess_poly_move_to(&builder->poly, x, y);
}

void path_builder_line_to(PathBuilder* builder, double x, double y) {
    RELEASE_ASSERT(builder);

    builder->x = x;
    builder->y = y;
    tess_poly_line_to(&builder->poly, x, y);
}

Canvas*
path_builder_render(PathBuilder* builder, uint32_t width, uint32_t height) {
    RELEASE_ASSERT(builder);

    Canvas* canvas = canvas_new(builder->arena, width, height, 0xffffffff);
    tess_poly_tessellate(&builder->poly);
    tess_poly_render(&builder->poly, canvas);

    return canvas;
}
