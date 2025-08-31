#pragma once

/// A 3x3 matrix
typedef struct {
    double m00;
    double m01;
    double m02;
    double m10;
    double m11;
    double m12;
    double m20;
    double m21;
    double m22;
} GeomMat3;

/// Creates a new 3x3 identity matrix
GeomMat3 geom_mat3_identity();
