#include "canvas/path_builder.h"

#include <math.h>

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

PathBuilderOptions path_builder_options_default(void) {
    return (PathBuilderOptions) {
        .flatten_curves = false,
        .quad_flatness = 0.05,
        .quad_max_depth = 24,
        .cubic_flatness = 0.05,
        .cubic_max_depth = 24,
    };
}

PathBuilderOptions path_builder_options_flattened(void) {
    PathBuilderOptions options = path_builder_options_default();
    options.flatten_curves = true;
    return options;
}

static void path_builder_validate_options(PathBuilderOptions options) {
    if (!options.flatten_curves) {
        return;
    }

    RELEASE_ASSERT(options.quad_flatness > 1e-9);
    RELEASE_ASSERT(options.quad_max_depth >= 0);
    RELEASE_ASSERT(options.cubic_flatness > 1e-9);
    RELEASE_ASSERT(options.cubic_max_depth >= 0);
}

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

static PathContour* path_builder_active_contour(PathBuilder* builder) {
    RELEASE_ASSERT(builder);

    size_t num_contours = path_contour_vec_len(builder->contours);
    RELEASE_ASSERT(num_contours != 0, "No active contour");

    PathContour* contour = NULL;
    RELEASE_ASSERT(
        path_contour_vec_get(builder->contours, num_contours - 1, &contour)
    );

    return contour;
}

static GeomVec2 path_builder_active_position(PathBuilder* builder) {
    PathContour* contour = path_builder_active_contour(builder);

    size_t num_segments = path_contour_len(contour);
    RELEASE_ASSERT(num_segments != 0, "No active segment");

    PathContourSegment segment;
    RELEASE_ASSERT(path_contour_get(contour, num_segments - 1, &segment));

    return path_contour_segment_end(segment);
}

static void path_builder_append_line(PathBuilder* builder, GeomVec2 point) {
    PathContour* contour = path_builder_active_contour(builder);

    path_contour_push(
        contour,
        (PathContourSegment) {.type = PATH_CONTOUR_SEGMENT_TYPE_LINE,
                              .value.line = point}
    );
}

static double path_point_distance_sq_to_segment(
    GeomVec2 point,
    GeomVec2 start,
    GeomVec2 end
) {
    GeomVec2 segment = geom_vec2_sub(end, start);
    double segment_len_sq = geom_vec2_len_sq(segment);
    if (segment_len_sq <= 1e-18) {
        return geom_vec2_len_sq(geom_vec2_sub(point, start));
    }

    double t =
        geom_vec2_dot(geom_vec2_sub(point, start), segment) / segment_len_sq;
    if (t < 0.0) {
        t = 0.0;
    } else if (t > 1.0) {
        t = 1.0;
    }

    GeomVec2 closest = geom_vec2_lerp(start, end, t);
    return geom_vec2_len_sq(geom_vec2_sub(point, closest));
}

static bool path_quad_is_flat_enough(
    GeomVec2 start,
    GeomVec2 control,
    GeomVec2 end,
    double flatness
) {
    // For a quadratic Bezier, the control-point weight peaks at 0.5.
    // If control-to-segment distance <= 2 * tolerance, the entire curve
    // is guaranteed within tolerance of the chord segment.
    double control_dist_sq =
        path_point_distance_sq_to_segment(control, start, end);
    double max_control_dist = flatness * 2.0;
    return control_dist_sq <= max_control_dist * max_control_dist;
}

static void path_builder_append_flattened_quad_recursive(
    PathBuilder* builder,
    GeomVec2 start,
    GeomVec2 control,
    GeomVec2 end,
    int depth
) {
    RELEASE_ASSERT(builder);

    bool flat_enough = path_quad_is_flat_enough(
        start,
        control,
        end,
        builder->options.quad_flatness
    );
    if (flat_enough) {
        path_builder_append_line(builder, end);
        return;
    }
    if (depth >= builder->options.quad_max_depth) {
        path_builder_append_line(builder, end);
        return;
    }

    GeomVec2 c1 = geom_vec2_lerp(start, control, 0.5);
    GeomVec2 c2 = geom_vec2_lerp(control, end, 0.5);
    GeomVec2 split = geom_vec2_lerp(c1, c2, 0.5);

    path_builder_append_flattened_quad_recursive(
        builder,
        start,
        c1,
        split,
        depth + 1
    );
    path_builder_append_flattened_quad_recursive(
        builder,
        split,
        c2,
        end,
        depth + 1
    );
}

static bool path_cubic_is_flat_enough(
    GeomVec2 start,
    GeomVec2 control_a,
    GeomVec2 control_b,
    GeomVec2 end,
    double flatness
) {
    double control_a_dist_sq =
        path_point_distance_sq_to_segment(control_a, start, end);
    double control_b_dist_sq =
        path_point_distance_sq_to_segment(control_b, start, end);
    double max_control_dist_sq = fmax(control_a_dist_sq, control_b_dist_sq);
    double max_control_dist = flatness * (4.0 / 3.0);
    return max_control_dist_sq <= max_control_dist * max_control_dist;
}

static void path_builder_append_flattened_cubic_recursive(
    PathBuilder* builder,
    GeomVec2 start,
    GeomVec2 control_a,
    GeomVec2 control_b,
    GeomVec2 end,
    int depth
) {
    RELEASE_ASSERT(builder);

    bool flat_enough = path_cubic_is_flat_enough(
        start,
        control_a,
        control_b,
        end,
        builder->options.cubic_flatness
    );
    if (flat_enough) {
        path_builder_append_line(builder, end);
        return;
    }
    if (depth >= builder->options.cubic_max_depth) {
        path_builder_append_line(builder, end);
        return;
    }

    GeomVec2 p01 = geom_vec2_lerp(start, control_a, 0.5);
    GeomVec2 p12 = geom_vec2_lerp(control_a, control_b, 0.5);
    GeomVec2 p23 = geom_vec2_lerp(control_b, end, 0.5);
    GeomVec2 p012 = geom_vec2_lerp(p01, p12, 0.5);
    GeomVec2 p123 = geom_vec2_lerp(p12, p23, 0.5);
    GeomVec2 split = geom_vec2_lerp(p012, p123, 0.5);

    path_builder_append_flattened_cubic_recursive(
        builder,
        start,
        p01,
        p012,
        split,
        depth + 1
    );
    path_builder_append_flattened_cubic_recursive(
        builder,
        split,
        p123,
        p23,
        end,
        depth + 1
    );
}

PathBuilder* path_builder_new(Arena* arena) {
    return path_builder_new_with_options(arena, path_builder_options_default());
}

PathBuilder*
path_builder_new_with_options(Arena* arena, PathBuilderOptions options) {
    RELEASE_ASSERT(arena);
    path_builder_validate_options(options);

    PathBuilder* builder = arena_alloc(arena, sizeof(PathBuilder));
    builder->arena = arena;
    builder->contours = path_contour_vec_new(arena);
    builder->options = options;

    return builder;
}

PathBuilder* path_builder_clone(Arena* arena, const PathBuilder* to_clone) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(to_clone);

    PathBuilder* builder =
        path_builder_new_with_options(arena, to_clone->options);

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

    PathContour* contour = path_builder_active_contour(builder);

    size_t contour_len = path_contour_len(contour);
    if (contour_len == 0) {
        return;
    }

    PathContourSegment first_segment;
    RELEASE_ASSERT(path_contour_get(contour, 0, &first_segment));
    RELEASE_ASSERT(first_segment.type == PATH_CONTOUR_SEGMENT_TYPE_START);
    GeomVec2 start = first_segment.value.start;

    PathContourSegment last_segment;
    RELEASE_ASSERT(path_contour_get(contour, contour_len - 1, &last_segment));
    GeomVec2 end = path_contour_segment_end(last_segment);

    if (!geom_vec2_equal_eps(start, end, 1e-9)) {
        path_builder_append_line(builder, start);
    }

    PathContour* new_contour = path_contour_new(builder->arena);
    path_contour_push(
        new_contour,
        (PathContourSegment) {.type = PATH_CONTOUR_SEGMENT_TYPE_START,
                              .value.start = start}
    );

    path_contour_vec_push(builder->contours, new_contour);
}

GeomVec2 path_builder_position(PathBuilder* builder) {
    RELEASE_ASSERT(builder);
    return path_builder_active_position(builder);
}

void path_builder_line_to(PathBuilder* builder, GeomVec2 point) {
    RELEASE_ASSERT(builder);
    path_builder_append_line(builder, point);
}

void path_builder_quad_bezier_to(
    PathBuilder* builder,
    GeomVec2 end,
    GeomVec2 control
) {
    RELEASE_ASSERT(builder);

    if (!builder->options.flatten_curves) {
        PathContour* contour = path_builder_active_contour(builder);
        path_contour_push(
            contour,
            (PathContourSegment) {
                .type = PATH_CONTOUR_SEGMENT_TYPE_QUAD_BEZIER,
                .value.quad_bezier = (PathQuadBezier) {
                                                       .control = control,
                                                       .end = end,
                                                       },
        }
        );
        return;
    }

    path_builder_append_flattened_quad_recursive(
        builder,
        path_builder_active_position(builder),
        control,
        end,
        0
    );
}

void path_builder_cubic_bezier_to(
    PathBuilder* builder,
    GeomVec2 end,
    GeomVec2 control_a,
    GeomVec2 control_b
) {
    RELEASE_ASSERT(builder);

    if (!builder->options.flatten_curves) {
        PathContour* contour = path_builder_active_contour(builder);
        path_contour_push(
            contour,
            (PathContourSegment) {
                .type = PATH_CONTOUR_SEGMENT_TYPE_CUBIC_BEZIER,
                .value.cubic_bezier = (PathCubicBezier) {
                                                         .control_a = control_a,
                                                         .control_b = control_b,
                                                         .end = end,
                                                         },
        }
        );
        return;
    }

    path_builder_append_flattened_cubic_recursive(
        builder,
        path_builder_active_position(builder),
        control_a,
        control_b,
        end,
        0
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

#ifdef TEST

#include "test/test.h"

static PathContour* path_builder_test_first_contour(PathBuilder* builder) {
    PathContour* contour = NULL;
    RELEASE_ASSERT(path_contour_vec_get(builder->contours, 0, &contour));
    return contour;
}

static GeomVec2 path_builder_test_eval_quad(
    GeomVec2 start,
    GeomVec2 control,
    GeomVec2 end,
    double t
) {
    double u = 1.0 - t;
    GeomVec2 a = geom_vec2_scale(start, u * u);
    GeomVec2 b = geom_vec2_scale(control, 2.0 * u * t);
    GeomVec2 c = geom_vec2_scale(end, t * t);
    return geom_vec2_add(geom_vec2_add(a, b), c);
}

static GeomVec2 path_builder_test_eval_cubic(
    GeomVec2 start,
    GeomVec2 control_a,
    GeomVec2 control_b,
    GeomVec2 end,
    double t
) {
    double u = 1.0 - t;
    GeomVec2 a = geom_vec2_scale(start, u * u * u);
    GeomVec2 b = geom_vec2_scale(control_a, 3.0 * u * u * t);
    GeomVec2 c = geom_vec2_scale(control_b, 3.0 * u * t * t);
    GeomVec2 d = geom_vec2_scale(end, t * t * t);
    return geom_vec2_add(geom_vec2_add(a, b), geom_vec2_add(c, d));
}

static double path_builder_test_min_distance_sq_to_polyline(
    const PathContour* contour,
    GeomVec2 point
) {
    RELEASE_ASSERT(contour);

    PathContourSegment start_segment;
    RELEASE_ASSERT(path_contour_get(contour, 0, &start_segment));
    RELEASE_ASSERT(start_segment.type == PATH_CONTOUR_SEGMENT_TYPE_START);
    GeomVec2 prev = start_segment.value.start;

    bool has_line = false;
    double min_distance_sq = HUGE_VAL;
    for (size_t idx = 1; idx < path_contour_len(contour); idx++) {
        PathContourSegment segment;
        RELEASE_ASSERT(path_contour_get(contour, idx, &segment));
        RELEASE_ASSERT(segment.type == PATH_CONTOUR_SEGMENT_TYPE_LINE);

        double distance_sq =
            path_point_distance_sq_to_segment(point, prev, segment.value.line);
        if (distance_sq < min_distance_sq) {
            min_distance_sq = distance_sq;
        }
        prev = segment.value.line;
        has_line = true;
    }

    RELEASE_ASSERT(has_line);
    return min_distance_sq;
}

TEST_FUNC(test_path_builder_options_flatten_disabled_keeps_quad) {
    Arena* arena = arena_new(1024);

    PathBuilder* path =
        path_builder_new_with_options(arena, path_builder_options_default());
    path_builder_new_contour(path, geom_vec2_new(0.0, 0.0));
    path_builder_quad_bezier_to(
        path,
        geom_vec2_new(1.0, 0.0),
        geom_vec2_new(0.5, 1.0)
    );

    PathContour* contour = path_builder_test_first_contour(path);
    TEST_ASSERT_EQ((unsigned long)path_contour_len(contour), 2UL);

    PathContourSegment segment;
    RELEASE_ASSERT(path_contour_get(contour, 1, &segment));
    TEST_ASSERT_EQ(
        (int)segment.type,
        (int)PATH_CONTOUR_SEGMENT_TYPE_QUAD_BEZIER
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_path_builder_close_contour_adds_closing_line) {
    Arena* arena = arena_new(1024);

    PathBuilder* path = path_builder_new(arena);
    path_builder_new_contour(path, geom_vec2_new(0.0, 0.0));
    path_builder_line_to(path, geom_vec2_new(1.0, 0.0));
    path_builder_line_to(path, geom_vec2_new(1.0, 1.0));
    path_builder_close_contour(path);

    TEST_ASSERT_EQ((unsigned long)path_contour_vec_len(path->contours), 2UL);

    PathContour* first = NULL;
    RELEASE_ASSERT(path_contour_vec_get(path->contours, 0, &first));
    TEST_ASSERT_EQ((unsigned long)path_contour_len(first), 4UL);

    PathContourSegment closing_segment;
    RELEASE_ASSERT(path_contour_get(first, 3, &closing_segment));
    TEST_ASSERT_EQ(
        (int)closing_segment.type,
        (int)PATH_CONTOUR_SEGMENT_TYPE_LINE
    );
    TEST_ASSERT(geom_vec2_equal_eps(
        closing_segment.value.line,
        geom_vec2_new(0.0, 0.0),
        1e-9
    ));

    TEST_ASSERT(geom_vec2_equal_eps(
        path_builder_position(path),
        geom_vec2_new(0.0, 0.0),
        1e-9
    ));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_path_builder_close_contour_keeps_existing_closure) {
    Arena* arena = arena_new(1024);

    PathBuilder* path = path_builder_new(arena);
    path_builder_new_contour(path, geom_vec2_new(0.0, 0.0));
    path_builder_line_to(path, geom_vec2_new(1.0, 0.0));
    path_builder_line_to(path, geom_vec2_new(0.0, 0.0));
    path_builder_close_contour(path);

    TEST_ASSERT_EQ((unsigned long)path_contour_vec_len(path->contours), 2UL);

    PathContour* first = NULL;
    RELEASE_ASSERT(path_contour_vec_get(path->contours, 0, &first));
    TEST_ASSERT_EQ((unsigned long)path_contour_len(first), 3UL);

    TEST_ASSERT(geom_vec2_equal_eps(
        path_builder_position(path),
        geom_vec2_new(0.0, 0.0),
        1e-9
    ));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_path_builder_options_flatten_enabled_flattens_quad) {
    Arena* arena = arena_new(1024);

    PathBuilderOptions options = path_builder_options_flattened();
    options.quad_flatness = 0.01;
    options.quad_max_depth = 16;

    PathBuilder* path = path_builder_new_with_options(arena, options);
    path_builder_new_contour(path, geom_vec2_new(0.0, 0.0));
    path_builder_quad_bezier_to(
        path,
        geom_vec2_new(1.0, 0.0),
        geom_vec2_new(0.5, 1.0)
    );

    PathContour* contour = path_builder_test_first_contour(path);
    TEST_ASSERT((unsigned long)path_contour_len(contour) > 2UL);

    for (size_t idx = 1; idx < path_contour_len(contour); idx++) {
        PathContourSegment segment;
        RELEASE_ASSERT(path_contour_get(contour, idx, &segment));
        TEST_ASSERT_EQ((int)segment.type, (int)PATH_CONTOUR_SEGMENT_TYPE_LINE);
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_path_builder_options_flatten_enabled_quad_within_tolerance) {
    Arena* arena = arena_new(4096);

    PathBuilderOptions options = path_builder_options_flattened();
    options.quad_flatness = 0.005;
    options.quad_max_depth = 24;

    GeomVec2 start = geom_vec2_new(0.0, 0.0);
    GeomVec2 control = geom_vec2_new(0.2, 1.3);
    GeomVec2 end = geom_vec2_new(1.0, -0.2);

    PathBuilder* path = path_builder_new_with_options(arena, options);
    path_builder_new_contour(path, start);
    path_builder_quad_bezier_to(path, end, control);

    PathContour* contour = path_builder_test_first_contour(path);
    TEST_ASSERT((unsigned long)path_contour_len(contour) > 2UL);

    const size_t sample_count = 10000; // dt = 1e-4
    double tolerance = options.quad_flatness + 1e-6;
    double tolerance_sq = tolerance * tolerance;
    for (size_t sample_idx = 0; sample_idx <= sample_count; sample_idx++) {
        double t = (double)sample_idx / (double)sample_count;
        GeomVec2 sample = path_builder_test_eval_quad(start, control, end, t);
        double min_distance_sq =
            path_builder_test_min_distance_sq_to_polyline(contour, sample);
        TEST_ASSERT(min_distance_sq <= tolerance_sq);
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_path_builder_options_flatten_enabled_quad_tolerance_boundary) {
    Arena* arena = arena_new(1024);

    PathBuilderOptions options = path_builder_options_flattened();
    options.quad_flatness = 0.05;
    options.quad_max_depth = 16;

    PathBuilder* path = path_builder_new_with_options(arena, options);
    path_builder_new_contour(path, geom_vec2_new(0.0, 0.0));
    // Control point is 0.1 above the chord; quadratic max deviation is half.
    path_builder_quad_bezier_to(
        path,
        geom_vec2_new(1.0, 0.0),
        geom_vec2_new(0.5, 0.1)
    );

    PathContour* contour = path_builder_test_first_contour(path);
    TEST_ASSERT_EQ((unsigned long)path_contour_len(contour), 2UL);

    PathContourSegment segment;
    RELEASE_ASSERT(path_contour_get(contour, 1, &segment));
    TEST_ASSERT_EQ((int)segment.type, (int)PATH_CONTOUR_SEGMENT_TYPE_LINE);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_path_builder_options_flatten_enabled_cubic_within_tolerance) {
    Arena* arena = arena_new(4096);

    PathBuilderOptions options = path_builder_options_flattened();
    options.cubic_flatness = 0.005;
    options.cubic_max_depth = 24;

    GeomVec2 start = geom_vec2_new(0.0, 0.0);
    GeomVec2 control_a = geom_vec2_new(0.25, 1.6);
    GeomVec2 control_b = geom_vec2_new(0.75, -1.2);
    GeomVec2 end = geom_vec2_new(1.0, 0.2);

    PathBuilder* path = path_builder_new_with_options(arena, options);
    path_builder_new_contour(path, start);
    path_builder_cubic_bezier_to(path, end, control_a, control_b);

    PathContour* contour = path_builder_test_first_contour(path);
    TEST_ASSERT((unsigned long)path_contour_len(contour) > 2UL);

    const size_t sample_count = 10000; // dt = 1e-4
    double tolerance = options.cubic_flatness + 1e-6;
    double tolerance_sq = tolerance * tolerance;
    for (size_t sample_idx = 0; sample_idx <= sample_count; sample_idx++) {
        double t = (double)sample_idx / (double)sample_count;
        GeomVec2 sample =
            path_builder_test_eval_cubic(start, control_a, control_b, end, t);
        double min_distance_sq =
            path_builder_test_min_distance_sq_to_polyline(contour, sample);
        TEST_ASSERT(min_distance_sq <= tolerance_sq);
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(
    test_path_builder_options_flatten_enabled_quad_within_tolerance_stress
) {
    Arena* arena = arena_new(8192);

    PathBuilderOptions options = path_builder_options_flattened();
    options.quad_flatness = 0.2;
    options.quad_max_depth = 24;

    struct {
        GeomVec2 start;
        GeomVec2 control;
        GeomVec2 end;
    } cases[] = {
        { .start = geom_vec2_new(0.0, 0.0),
         .control = geom_vec2_new(0.5,  1.0),
         .end = geom_vec2_new(1.0,  0.0)},
        {.start = geom_vec2_new(-2.0, 0.0),
         .control = geom_vec2_new(4.0,  8.0),
         .end = geom_vec2_new(6.0, -1.0)},
        { .start = geom_vec2_new(0.0, 0.0),
         .control = geom_vec2_new(3.5, 18.0),
         .end = geom_vec2_new(9.0,  0.0)},
    };

    const size_t sample_count = 20000;
    double tolerance = options.quad_flatness + 1e-6;
    double tolerance_sq = tolerance * tolerance;

    for (size_t case_idx = 0; case_idx < sizeof(cases) / sizeof(cases[0]);
         case_idx++) {
        PathBuilder* path = path_builder_new_with_options(arena, options);
        path_builder_new_contour(path, cases[case_idx].start);
        path_builder_quad_bezier_to(
            path,
            cases[case_idx].end,
            cases[case_idx].control
        );

        PathContour* contour = path_builder_test_first_contour(path);
        for (size_t sample_idx = 0; sample_idx <= sample_count; sample_idx++) {
            double t = (double)sample_idx / (double)sample_count;
            GeomVec2 sample = path_builder_test_eval_quad(
                cases[case_idx].start,
                cases[case_idx].control,
                cases[case_idx].end,
                t
            );
            double min_distance_sq =
                path_builder_test_min_distance_sq_to_polyline(contour, sample);
            TEST_ASSERT(min_distance_sq <= tolerance_sq);
        }
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(
    test_path_builder_options_flatten_enabled_cubic_within_tolerance_stress
) {
    Arena* arena = arena_new(8192);

    PathBuilderOptions options = path_builder_options_flattened();
    options.cubic_flatness = 0.2;
    options.cubic_max_depth = 24;

    struct {
        GeomVec2 start;
        GeomVec2 control_a;
        GeomVec2 control_b;
        GeomVec2 end;
    } cases[] = {
        { .start = geom_vec2_new(0.0,  0.0),
         .control_a = geom_vec2_new(0.25,  1.6),
         .control_b = geom_vec2_new(0.75,  -1.2),
         .end = geom_vec2_new(1.0, 0.2) },
        { .start = geom_vec2_new(0.0,  0.0),
         .control_a = geom_vec2_new(3.0,  0.0),
         .control_b = geom_vec2_new(-2.0,   0.0),
         .end = geom_vec2_new(1.0, 0.0) },
        {.start = geom_vec2_new(-4.0, -1.0),
         .control_a = geom_vec2_new(2.0, 16.0),
         .control_b = geom_vec2_new(9.0, -12.0),
         .end = geom_vec2_new(12.0, 0.0)},
    };

    const size_t sample_count = 20000;
    double tolerance = options.cubic_flatness + 1e-6;
    double tolerance_sq = tolerance * tolerance;

    for (size_t case_idx = 0; case_idx < sizeof(cases) / sizeof(cases[0]);
         case_idx++) {
        PathBuilder* path = path_builder_new_with_options(arena, options);
        path_builder_new_contour(path, cases[case_idx].start);
        path_builder_cubic_bezier_to(
            path,
            cases[case_idx].end,
            cases[case_idx].control_a,
            cases[case_idx].control_b
        );

        PathContour* contour = path_builder_test_first_contour(path);
        for (size_t sample_idx = 0; sample_idx <= sample_count; sample_idx++) {
            double t = (double)sample_idx / (double)sample_count;
            GeomVec2 sample = path_builder_test_eval_cubic(
                cases[case_idx].start,
                cases[case_idx].control_a,
                cases[case_idx].control_b,
                cases[case_idx].end,
                t
            );
            double min_distance_sq =
                path_builder_test_min_distance_sq_to_polyline(contour, sample);
            TEST_ASSERT(min_distance_sq <= tolerance_sq);
        }
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_path_builder_options_flatten_enabled_quad_depth_cap) {
    Arena* arena = arena_new(1024);

    PathBuilderOptions options = path_builder_options_flattened();
    options.quad_flatness = 1e-8;
    options.quad_max_depth = 0;

    PathBuilder* path = path_builder_new_with_options(arena, options);
    path_builder_new_contour(path, geom_vec2_new(0.0, 0.0));
    path_builder_quad_bezier_to(
        path,
        geom_vec2_new(1.0, 0.0),
        geom_vec2_new(0.5, 1.0)
    );

    PathContour* contour = path_builder_test_first_contour(path);
    TEST_ASSERT_EQ((unsigned long)path_contour_len(contour), 2UL);

    PathContourSegment segment;
    RELEASE_ASSERT(path_contour_get(contour, 1, &segment));
    TEST_ASSERT_EQ((int)segment.type, (int)PATH_CONTOUR_SEGMENT_TYPE_LINE);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_path_builder_options_flatten_enabled_cubic_collinear_overshoot) {
    Arena* arena = arena_new(1024);

    PathBuilderOptions options = path_builder_options_flattened();
    options.cubic_flatness = 0.01;
    options.cubic_max_depth = 16;

    PathBuilder* path = path_builder_new_with_options(arena, options);
    path_builder_new_contour(path, geom_vec2_new(0.0, 0.0));
    path_builder_cubic_bezier_to(
        path,
        geom_vec2_new(1.0, 0.0),
        geom_vec2_new(3.0, 0.0),
        geom_vec2_new(-2.0, 0.0)
    );

    PathContour* contour = path_builder_test_first_contour(path);
    TEST_ASSERT((unsigned long)path_contour_len(contour) > 2UL);

    for (size_t idx = 1; idx < path_contour_len(contour); idx++) {
        PathContourSegment segment;
        RELEASE_ASSERT(path_contour_get(contour, idx, &segment));
        TEST_ASSERT_EQ((int)segment.type, (int)PATH_CONTOUR_SEGMENT_TYPE_LINE);
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_path_builder_options_flatten_enabled_cubic_depth_cap) {
    Arena* arena = arena_new(1024);

    PathBuilderOptions options = path_builder_options_flattened();
    options.cubic_flatness = 1e-8;
    options.cubic_max_depth = 0;

    PathBuilder* path = path_builder_new_with_options(arena, options);
    path_builder_new_contour(path, geom_vec2_new(0.0, 0.0));
    path_builder_cubic_bezier_to(
        path,
        geom_vec2_new(1.0, 0.0),
        geom_vec2_new(0.0, 1.0),
        geom_vec2_new(1.0, 1.0)
    );

    PathContour* contour = path_builder_test_first_contour(path);
    TEST_ASSERT_EQ((unsigned long)path_contour_len(contour), 2UL);

    PathContourSegment segment;
    RELEASE_ASSERT(path_contour_get(contour, 1, &segment));
    TEST_ASSERT_EQ((int)segment.type, (int)PATH_CONTOUR_SEGMENT_TYPE_LINE);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif
