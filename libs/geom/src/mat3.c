#include "geom/mat3.h"

GeomMat3 geom_mat3_new(
    double m00,
    double m01,
    double m02,
    double m10,
    double m11,
    double m12,
    double m20,
    double m21,
    double m22
) {
    return (GeomMat3) {
        .mat = {{m00, m01, m02}, {m10, m11, m12}, {m20, m21, m22}}
    };
}

GeomMat3 geom_mat3_identity(void) {
    return (GeomMat3) {
        .mat = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}
    };
}

GeomMat3 geom_mat3_translate(double x, double y) {
    return (GeomMat3) {
        .mat = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {x, y, 1.0}}
    };
}

GeomMat3 geom_mat3_mul(GeomMat3 lhs, GeomMat3 rhs) {
    GeomMat3 result = {0};

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                result.mat[i][j] += lhs.mat[i][k] * rhs.mat[k][j];
            }
        }
    }

    return result;
}
