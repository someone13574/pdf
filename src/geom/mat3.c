#include "geom/mat3.h"

GeomMat3 geom_mat3_identity(void) {
    return (GeomMat3) {.m00 = 1.0, .m11 = 1.0, .m22 = 1.0};
}
