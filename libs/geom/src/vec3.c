#include "geom/vec3.h"

#include <math.h>

GeomVec3 geom_vec3_new(double x, double y, double z) {
    return (GeomVec3) {.x = x, .y = y, .z = z};
}

GeomVec3 geom_vec3_scalar(double v) {
    return geom_vec3_new(v, v, v);
}

GeomVec3 geom_vec3_add(GeomVec3 lhs, GeomVec3 rhs) {
    return geom_vec3_new(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
}

GeomVec3 geom_vec3_sub(GeomVec3 lhs, GeomVec3 rhs) {
    return geom_vec3_new(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
}

GeomVec3 geom_vec3_mul(GeomVec3 lhs, GeomVec3 rhs) {
    return geom_vec3_new(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z);
}

GeomVec3 geom_vec3_div(GeomVec3 lhs, GeomVec3 rhs) {
    return geom_vec3_new(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z);
}

GeomVec3 geom_vec3_pow(GeomVec3 lhs, GeomVec3 rhs) {
    return geom_vec3_new(
        pow(lhs.x, rhs.x),
        pow(lhs.y, rhs.y),
        pow(lhs.z, rhs.z)
    );
}

static double clamp01(double value) {
    return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

GeomVec3 geom_vec3_clamp(GeomVec3 value) {
    return geom_vec3_new(
        clamp01(value.x),
        clamp01(value.y),
        clamp01(value.z)
    );
}

GeomVec3 geom_vec3_transform(GeomVec3 lhs, GeomMat3 rhs) {
    return (GeomVec3) {.x = lhs.x * rhs.mat[0][0] + lhs.y * rhs.mat[1][0]
                          + lhs.z * rhs.mat[2][0],
                       .y = lhs.x * rhs.mat[0][1] + lhs.y * rhs.mat[1][1]
                          + lhs.z * rhs.mat[2][1],
                       .z = lhs.x * rhs.mat[0][2] + lhs.y * rhs.mat[1][2]
                          + lhs.z * rhs.mat[2][2]};
}

#ifdef TEST

#include "test/test.h"

TEST_FUNC(test_geom_vec3_clamp) {
    GeomVec3 clamped = geom_vec3_clamp(geom_vec3_new(-1.0, 0.25, 2.0));

    TEST_ASSERT_EQ(clamped.x, 0.0);
    TEST_ASSERT_EQ(clamped.y, 0.25);
    TEST_ASSERT_EQ(clamped.z, 1.0);

    return TEST_RESULT_PASS;
}

#endif
