#include "geom/rect.h"

#include <math.h>

#include "geom/vec2.h"

GeomRect geom_rect_new(GeomVec2 point_a, GeomVec2 point_b) {
    return (GeomRect) {.min = geom_vec2_min(point_a, point_b),
                       .max = geom_vec2_max(point_a, point_b)};
}

GeomRect geom_rect_new_centered(GeomVec2 center, GeomVec2 radius) {
    return (GeomRect) {
        .min = geom_vec2_new(center.x - radius.x, center.y - radius.y),
        .max = geom_vec2_new(center.x + radius.x, center.y + radius.y)
    };
}

GeomRect geom_rect_round(GeomRect rect) {
    rect.min.x = floor(rect.min.x);
    rect.min.y = floor(rect.min.y);
    rect.max.x = ceil(rect.max.x);
    rect.max.y = ceil(rect.max.y);

    return rect;
}

GeomVec2 geom_rect_size(GeomRect rect) {
    return geom_vec2_new(rect.max.x - rect.min.x, rect.max.y - rect.min.y);
}

GeomRect geom_rect_union(GeomRect a, GeomRect b) {
    return (GeomRect) {.min = geom_vec2_min(a.min, b.min),
                       .max = geom_vec2_max(a.max, b.max)};
}

GeomRect geom_rect_intersection(GeomRect a, GeomRect b) {
    return (GeomRect) {.min.x = fmax(a.min.x, b.min.x),
                       .min.y = fmax(a.min.y, b.min.y),
                       .max.x = fmin(a.max.x, b.max.x),
                       .max.y = fmin(a.max.y, b.max.y)};
}

double geom_rect_area(GeomRect rect) {
    return (rect.max.x - rect.min.x) * (rect.max.y - rect.min.y);
}

bool geom_rect_positive(GeomRect rect) {
    return rect.max.x > rect.min.x + 1e-6 && rect.max.y > rect.min.y + 1e-6;
}

GeomRect geom_rect_transform(GeomRect rect, GeomMat3 transform) {
    if (!geom_rect_positive(rect)) {
        return (
            GeomRect
        ) {.min.x = 0.0, .min.y = 0.0, .max.x = 0.0, .max.y = 0.0};
    }

    GeomVec2 a =
        geom_vec2_transform(geom_vec2_new(rect.min.x, rect.min.y), transform);
    GeomVec2 b =
        geom_vec2_transform(geom_vec2_new(rect.min.x, rect.max.y), transform);
    GeomVec2 c =
        geom_vec2_transform(geom_vec2_new(rect.max.x, rect.min.y), transform);
    GeomVec2 d =
        geom_vec2_transform(geom_vec2_new(rect.max.x, rect.max.y), transform);

    return (GeomRect) {
        .min = geom_vec2_min(geom_vec2_min(a, b), geom_vec2_min(c, d)),
        .max = geom_vec2_max(geom_vec2_max(a, b), geom_vec2_max(c, d))
    };
}
