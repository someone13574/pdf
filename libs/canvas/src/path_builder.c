#include "canvas/path_builder.h"

#include "arena/arena.h"
#include "geom/vec2.h"
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

GeomVec2 path_contour_segment_end(PathContourSegment segment) {
    switch (segment.type) {
        case PATH_CONTOUR_SEGMENT_TYPE_START: {
            return segment.value.start;
        }
        case PATH_CONTOUR_SEGMENT_TYPE_LINE: {
            return segment.value.line;
        }
        case PATH_CONTOUR_SEGMENT_TYPE_QUAD_BEZIER: {
            return segment.value.quad_bezier.end;
        }
        case PATH_CONTOUR_SEGMENT_TYPE_CUBIC_BEZIER: {
            return segment.value.cubic_bezier.end;
        }
    }

    return geom_vec2_new(0.0, 0.0);
}

PathBuilder* path_builder_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    PathBuilder* builder = arena_alloc(arena, sizeof(PathBuilder));
    builder->arena = arena;
    builder->contours = path_contour_vec_new(arena);

    return builder;
}

PathBuilder* path_builder_clone(Arena* arena, const PathBuilder* to_clone) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(to_clone);

    PathBuilder* builder = path_builder_new(arena);

    for (size_t contour_idx = 0;
         contour_idx < path_contour_vec_len(to_clone->contours);
         contour_idx++) {
        PathContour* contour_to_clone = NULL;
        RELEASE_ASSERT(path_contour_vec_get(
            to_clone->contours,
            contour_idx,
            &contour_to_clone
        ));

        PathContour* contour = path_contour_new(arena);

        for (size_t segment_idx = 0;
             segment_idx < path_contour_len(contour_to_clone);
             segment_idx++) {
            PathContourSegment* segment_to_clone = NULL;
            RELEASE_ASSERT(path_contour_get_ptr(
                contour_to_clone,
                segment_idx,
                &segment_to_clone
            ));

            path_contour_push(contour, *segment_to_clone);
        }
        path_contour_vec_push(builder->contours, contour);
    }

    return builder;
}

void path_builder_new_contour(PathBuilder* builder, GeomVec2 point) {
    RELEASE_ASSERT(builder);

    PathContour* contour = path_contour_new(builder->arena);
    path_contour_push(
        contour,
        (PathContourSegment) {.type = PATH_CONTOUR_SEGMENT_TYPE_START,
                              .value.start = point}
    );

    path_contour_vec_push(builder->contours, contour);
}

void path_builder_close_contour(PathBuilder* builder) {
    RELEASE_ASSERT(builder);

    size_t num_contours = path_contour_vec_len(builder->contours);
    RELEASE_ASSERT(num_contours != 0, "No active contour");

    PathContour* contour = NULL;
    RELEASE_ASSERT(
        path_contour_vec_get(builder->contours, num_contours - 1, &contour)
    );

    size_t contour_len = path_contour_len(contour);
    if (contour_len == 0) {
        return;
    }

    PathContourSegment segment;
    RELEASE_ASSERT(path_contour_get(contour, contour_len - 1, &segment));

    PathContour* new_contour = path_contour_new(builder->arena);
    path_contour_push(
        new_contour,
        (PathContourSegment) {.type = PATH_CONTOUR_SEGMENT_TYPE_START,
                              .value.start = path_contour_segment_end(segment)}
    );

    path_contour_vec_push(builder->contours, new_contour);
}

GeomVec2 path_builder_position(PathBuilder* builder) {
    RELEASE_ASSERT(builder);

    size_t num_contours = path_contour_vec_len(builder->contours);
    RELEASE_ASSERT(num_contours != 0, "No active contour");

    PathContour* contour = NULL;
    RELEASE_ASSERT(
        path_contour_vec_get(builder->contours, num_contours - 1, &contour)
    );

    size_t num_segments = path_contour_vec_len(builder->contours);
    RELEASE_ASSERT(num_segments != 0, "No active segment");

    PathContourSegment segment;
    RELEASE_ASSERT(path_contour_get(contour, num_segments - 1, &segment));

    switch (segment.type) {
        case PATH_CONTOUR_SEGMENT_TYPE_START: {
            return segment.value.start;
        }
        case PATH_CONTOUR_SEGMENT_TYPE_LINE: {
            return segment.value.line;
        }
        case PATH_CONTOUR_SEGMENT_TYPE_QUAD_BEZIER: {
            return segment.value.quad_bezier.end;
        }
        case PATH_CONTOUR_SEGMENT_TYPE_CUBIC_BEZIER: {
            return segment.value.cubic_bezier.end;
        }
    }

    LOG_PANIC("Unreachable");
}

void path_builder_line_to(PathBuilder* builder, GeomVec2 point) {
    RELEASE_ASSERT(builder);

    size_t num_contours = path_contour_vec_len(builder->contours);
    RELEASE_ASSERT(num_contours != 0, "No active contour");

    PathContour* contour = NULL;
    RELEASE_ASSERT(
        path_contour_vec_get(builder->contours, num_contours - 1, &contour)
    );

    path_contour_push(
        contour,
        (PathContourSegment) {.type = PATH_CONTOUR_SEGMENT_TYPE_LINE,
                              .value.line = point}
    );
}

void path_builder_quad_bezier_to(
    PathBuilder* builder,
    GeomVec2 end,
    GeomVec2 control
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
            .type = PATH_CONTOUR_SEGMENT_TYPE_QUAD_BEZIER,
            .value.quad_bezier =
                (PathQuadBezier) {.control = control, .end = end}
    }
    );
}

void path_builder_cubic_bezier_to(
    PathBuilder* builder,
    GeomVec2 end,
    GeomVec2 control_a,
    GeomVec2 control_b
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
            .type = PATH_CONTOUR_SEGMENT_TYPE_CUBIC_BEZIER,
            .value.cubic_bezier = (PathCubicBezier) {.control_a = control_a,
                                                     .control_b = control_b,
                                                     .end = end}
    }
    );
}

void path_builder_apply_transform(PathBuilder* path, GeomMat3 transform) {
    RELEASE_ASSERT(path);

    for (size_t contour_idx = 0;
         contour_idx < path_contour_vec_len(path->contours);
         contour_idx++) {
        PathContour* contour = NULL;
        RELEASE_ASSERT(
            path_contour_vec_get(path->contours, contour_idx, &contour)
        );

        for (size_t segment_idx = 0; segment_idx < path_contour_len(contour);
             segment_idx++) {
            PathContourSegment* segment = NULL;
            RELEASE_ASSERT(
                path_contour_get_ptr(contour, segment_idx, &segment)
            );

            switch (segment->type) {
                case PATH_CONTOUR_SEGMENT_TYPE_START: {
                    segment->value.start =
                        geom_vec2_transform(segment->value.start, transform);
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_LINE: {
                    segment->value.line =
                        geom_vec2_transform(segment->value.line, transform);
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_QUAD_BEZIER: {
                    segment->value.quad_bezier.control = geom_vec2_transform(
                        segment->value.quad_bezier.control,
                        transform
                    );
                    segment->value.quad_bezier.end = geom_vec2_transform(
                        segment->value.quad_bezier.end,
                        transform
                    );
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_CUBIC_BEZIER: {
                    segment->value.cubic_bezier.control_a = geom_vec2_transform(
                        segment->value.cubic_bezier.control_a,
                        transform
                    );
                    segment->value.cubic_bezier.control_b = geom_vec2_transform(
                        segment->value.cubic_bezier.control_b,
                        transform
                    );
                    segment->value.cubic_bezier.end = geom_vec2_transform(
                        segment->value.cubic_bezier.end,
                        transform
                    );
                    break;
                }
            }
        }
    }
}
