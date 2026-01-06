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

GeomVec3 geom_vec3_transform(GeomVec3 lhs, GeomMat3 rhs) {
    return (GeomVec3) {.x = lhs.x * rhs.mat[0][0] + lhs.y * rhs.mat[1][0]
                          + lhs.z * rhs.mat[2][0],
                       .y = lhs.x * rhs.mat[0][1] + lhs.y * rhs.mat[1][1]
                          + lhs.z * rhs.mat[2][1],
                       .z = lhs.x * rhs.mat[0][2] + lhs.y * rhs.mat[1][2]
                          + lhs.z * rhs.mat[2][2]};
}
