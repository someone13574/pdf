#pragma once

/// A 3x3 matrix
typedef struct {
    double mat[3][3];
} GeomMat3;

/// Creates a new 3x3 matrix
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
);

/// Creates a new 3x3 matrix following the 6-number PDF convention used by the
/// Tm text-positioning operator
GeomMat3
geom_mat3_new_pdf(double a, double b, double c, double d, double e, double f);

/// Creates a new 3x3 identity matrix
GeomMat3 geom_mat3_identity(void);

/// Creates a translation matrix
GeomMat3 geom_mat3_translate(double x, double y);

/// Multiply two 3x3 matrices
GeomMat3 geom_mat3_mul(GeomMat3 lhs, GeomMat3 rhs);
