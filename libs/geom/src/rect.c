#include "geom/rect.h"

#include <math.h>

#include "geom/vec2.h"

GeomRect geom_rect_new(GeomVec2 point_a, GeomVec2 point_b) {
    return (GeomRect) {.min.x = fmin(point_a.x, point_b.x),
                       .min.y = fmin(point_a.y, point_b.y),
                       .max.x = fmax(point_a.x, point_b.x),
                       .max.y = fmax(point_a.y, point_b.y)};
}

GeomVec2 geom_rect_size(GeomRect rect) {
    return geom_vec2_new(rect.max.x - rect.min.x, rect.max.y - rect.min.y);
}
