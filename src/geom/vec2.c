#include "geom/vec2.h"

GeomVec2 geom_vec2_new(double x, double y) {
    return (GeomVec2) {.x = x, .y = y};
}
