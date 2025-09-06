#include "canvas/path_builder.h"

#include "arena/arena.h"
#include "logger/log.h"
#include "path_builder.h"

#define DVEC_NAME PathContour
#define DVEC_LOWERCASE_NAME path_contour
#define DVEC_TYPE PathContourSegment
#include "arena/dvec_impl.h"

#define DVEC_NAME PathContourVec
#define DVEC_LOWERCASE_NAME path_contour_vec
#define DVEC_TYPE PathContour*
#include "arena/dvec_impl.h"

PathBuilder* path_builder_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    PathBuilder* builder = arena_alloc(arena, sizeof(PathBuilder));
    builder->arena = arena;
    builder->contours = path_contour_vec_new(arena);

    return builder;
}

void path_builder_new_contour(PathBuilder* builder, double x, double y) {
    RELEASE_ASSERT(builder);

    PathContour* contour = path_contour_new(builder->arena);
    path_contour_push(
        contour,
        (PathContourSegment) {
            .type = PATH_CONTOUR_SEGMENT_TYPE_START,
            .data.start = (PathPoint) {.x = x, .y = y}
    }
    );

    path_contour_vec_push(builder->contours, contour);
}

void path_builder_line_to(PathBuilder* builder, double x, double y) {
    RELEASE_ASSERT(builder);

    size_t num_contours = path_contour_vec_len(builder->contours);
    RELEASE_ASSERT(num_contours != 0, "No active contour");

    PathContour* contour = NULL;
    RELEASE_ASSERT(
        path_contour_vec_get(builder->contours, num_contours - 1, &contour)
    );

    path_contour_push(
        contour,
        (PathContourSegment) {
            .type = PATH_CONTOUR_SEGMENT_TYPE_LINE,
            .data.line = (PathPoint) {.x = x, .y = y}
    }
    );
}

void path_builder_bezier_to(
    PathBuilder* builder,
    double x,
    double y,
    double cx,
    double cy
) {
    RELEASE_ASSERT(builder);

    size_t num_contours = path_contour_vec_len(builder->contours);
    RELEASE_ASSERT(num_contours != 0, "No active contour");

    PathContour* contour = NULL;
    RELEASE_ASSERT(
        path_contour_vec_get(builder->contours, num_contours - 1, &contour)
    );

    path_contour_push(
        contour,
        (PathContourSegment) {
            .type = PATH_CONTOUR_SEGMENT_TYPE_BEZIER,
            .data.bezier = (PathQuadBezier
            ) {.control.x = cx, .control.y = cy, .end.x = x, .end.y = y}
    }
    );
}
