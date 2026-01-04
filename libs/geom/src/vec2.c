#include "geom/vec2.h"

#include <math.h>

#include "geom/vec3.h"

GeomVec2 geom_vec2_new(double x, double y) {
    return (GeomVec2) {.x = x, .y = y};
}

GeomVec2 geom_vec2_add(GeomVec2 a, GeomVec2 b) {
    return geom_vec2_new(a.x + b.x, a.y + b.y);
}

GeomVec2 geom_vec2_min(GeomVec2 a, GeomVec2 b) {
    return geom_vec2_new(fmin(a.x, b.x), fmin(a.y, b.y));
}

GeomVec2 geom_vec2_max(GeomVec2 a, GeomVec2 b) {
    return geom_vec2_new(fmax(a.x, b.x), fmax(a.y, b.y));
}

GeomVec2 geom_vec2_transform(GeomVec2 lhs, GeomMat3 rhs) {
    GeomVec3 transformed =
        geom_vec3_transform(geom_vec3_new(lhs.x, lhs.y, 1.0), rhs);
    return geom_vec2_new(transformed.x, transformed.y);
}

GeomVec2 geom_vec2_transpose(GeomVec2 vec) {
    return geom_vec2_new(vec.y, vec.x);
}
