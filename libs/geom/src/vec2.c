#include "geom/vec2.h"

#include <math.h>

#include "geom/vec3.h"

GeomVec2 geom_vec2_new(double x, double y) {
    return (GeomVec2) {.x = x, .y = y};
}

GeomVec2 geom_vec2_add(GeomVec2 a, GeomVec2 b) {
    return geom_vec2_new(a.x + b.x, a.y + b.y);
}

GeomVec2 geom_vec2_sub(GeomVec2 a, GeomVec2 b) {
    return geom_vec2_new(a.x - b.x, a.y - b.y);
}

GeomVec2 geom_vec2_scale(GeomVec2 vec, double scalar) {
    return geom_vec2_new(vec.x * scalar, vec.y * scalar);
}

double geom_vec2_dot(GeomVec2 a, GeomVec2 b) {
    return a.x * b.x + a.y * b.y;
}

double geom_vec2_len_sq(GeomVec2 vec) {
    return geom_vec2_dot(vec, vec);
}

GeomVec2 geom_vec2_lerp(GeomVec2 a, GeomVec2 b, double t) {
    return geom_vec2_add(a, geom_vec2_scale(geom_vec2_sub(b, a), t));
}

GeomVec2 geom_vec2_min(GeomVec2 a, GeomVec2 b) {
    return geom_vec2_new(fmin(a.x, b.x), fmin(a.y, b.y));
}

GeomVec2 geom_vec2_max(GeomVec2 a, GeomVec2 b) {
    return geom_vec2_new(fmax(a.x, b.x), fmax(a.y, b.y));
}

bool geom_vec2_equal_eps(GeomVec2 a, GeomVec2 b, double epsilon) {
    return fabs(a.x - b.x) <= epsilon && fabs(a.y - b.y) <= epsilon;
}

GeomVec2 geom_vec2_transform(GeomVec2 lhs, GeomMat3 rhs) {
    GeomVec3 transformed =
        geom_vec3_transform(geom_vec3_new(lhs.x, lhs.y, 1.0), rhs);
    return geom_vec2_new(transformed.x, transformed.y);
}

GeomVec2 geom_vec2_transpose(GeomVec2 vec) {
    return geom_vec2_new(vec.y, vec.x);
}

#ifdef TEST

#include "test/test.h"

TEST_FUNC(test_geom_vec2_equal_eps) {
    TEST_ASSERT(geom_vec2_equal_eps(
        geom_vec2_new(1.0, 2.0),
        geom_vec2_new(1.0000001, 2.0000001),
        1e-3
    ));
    TEST_ASSERT(!geom_vec2_equal_eps(
        geom_vec2_new(1.0, 2.0),
        geom_vec2_new(1.1, 2.0),
        1e-3
    ));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_geom_vec2_lerp_and_len_sq) {
    GeomVec2 mid = geom_vec2_lerp(geom_vec2_new(0.0, 0.0), geom_vec2_new(2.0, 4.0), 0.5);
    TEST_ASSERT_EQ(mid.x, 1.0);
    TEST_ASSERT_EQ(mid.y, 2.0);

    GeomVec2 delta = geom_vec2_sub(geom_vec2_new(5.0, 6.0), geom_vec2_new(1.0, 2.0));
    TEST_ASSERT_EQ(geom_vec2_len_sq(delta), 32.0);

    return TEST_RESULT_PASS;
}

#endif
