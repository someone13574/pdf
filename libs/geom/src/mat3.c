#include "geom/mat3.h"

#include <math.h>

#include "geom/vec2.h"
#include "logger/log.h"

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

GeomMat3
geom_mat3_new_pdf(double a, double b, double c, double d, double e, double f) {
    return geom_mat3_new(a, b, 0.0, c, d, 0.0, e, f, 1.0);
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

double geom_mat3_minor(GeomMat3 mat, int y, int x) {
    RELEASE_ASSERT(x >= 0 && x < 3 && y >= 0 && y < 3);

    int x_a = x == 0 ? 1 : 0;
    int x_b = x == 2 ? 1 : 2;
    int y_a = y == 0 ? 1 : 0;
    int y_b = y == 2 ? 1 : 2;

    return mat.mat[y_a][x_a] * mat.mat[y_b][x_b]
         - mat.mat[y_a][x_b] * mat.mat[y_b][x_a];
}

double geom_mat3_det(GeomMat3 mat) {
    return mat.mat[0][0] * geom_mat3_minor(mat, 0, 0)
         - mat.mat[0][1] * geom_mat3_minor(mat, 0, 1)
         + mat.mat[0][2] * geom_mat3_minor(mat, 0, 2);
}

GeomMat3 geom_mat3_inverse(GeomMat3 mat) {
    double det = geom_mat3_det(mat);
    RELEASE_ASSERT(fabs(det) > 1e-6);

    return geom_mat3_new(
        geom_mat3_minor(mat, 0, 0) / det,
        -geom_mat3_minor(mat, 1, 0) / det,
        geom_mat3_minor(mat, 2, 0) / det,
        -geom_mat3_minor(mat, 0, 1) / det,
        geom_mat3_minor(mat, 1, 1) / det,
        -geom_mat3_minor(mat, 2, 1) / det,
        geom_mat3_minor(mat, 0, 2) / det,
        -geom_mat3_minor(mat, 1, 2) / det,
        geom_mat3_minor(mat, 2, 2) / det
    );
}

#ifdef TEST

#include "test/test.h"

TEST_FUNC(test_geom_mat3_new) {
    GeomMat3 mat = geom_mat3_new(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0);

    TEST_ASSERT_EQ(mat.mat[0][0], 1.0);
    TEST_ASSERT_EQ(mat.mat[0][1], 2.0);
    TEST_ASSERT_EQ(mat.mat[0][2], 3.0);
    TEST_ASSERT_EQ(mat.mat[1][0], 4.0);
    TEST_ASSERT_EQ(mat.mat[1][1], 5.0);
    TEST_ASSERT_EQ(mat.mat[1][2], 6.0);
    TEST_ASSERT_EQ(mat.mat[2][0], 7.0);
    TEST_ASSERT_EQ(mat.mat[2][1], 8.0);
    TEST_ASSERT_EQ(mat.mat[2][2], 9.0);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_geom_mat3_identity) {
    GeomMat3 mat = geom_mat3_identity();

    TEST_ASSERT_EQ(mat.mat[0][0], 1.0);
    TEST_ASSERT_EQ(mat.mat[0][1], 0.0);
    TEST_ASSERT_EQ(mat.mat[0][2], 0.0);
    TEST_ASSERT_EQ(mat.mat[1][0], 0.0);
    TEST_ASSERT_EQ(mat.mat[1][1], 1.0);
    TEST_ASSERT_EQ(mat.mat[1][2], 0.0);
    TEST_ASSERT_EQ(mat.mat[2][0], 0.0);
    TEST_ASSERT_EQ(mat.mat[2][1], 0.0);
    TEST_ASSERT_EQ(mat.mat[2][2], 1.0);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_geom_mat3_translate) {
    GeomMat3 transform = geom_mat3_translate(5.0, 3.0);
    GeomVec2 transformed =
        geom_vec2_transform(geom_vec2_new(8.0, 16.0), transform);

    TEST_ASSERT_EQ(transformed.x, 13.0);
    TEST_ASSERT_EQ(transformed.y, 19.0);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_geom_mat3_mul) {
    GeomMat3 a = geom_mat3_new(1.0, 2.0, 1.0, 0.0, 1.0, 0.0, 2.0, 3.0, 4.0);
    GeomMat3 b = geom_mat3_new(2.0, 5.0, 1.0, 6.0, 7.0, 3.0, 1.0, 8.0, 6.0);
    GeomMat3 result = geom_mat3_mul(a, b);

    TEST_ASSERT_EQ(result.mat[0][0], 15.0);
    TEST_ASSERT_EQ(result.mat[0][1], 27.0);
    TEST_ASSERT_EQ(result.mat[0][2], 13.0);
    TEST_ASSERT_EQ(result.mat[1][0], 6.0);
    TEST_ASSERT_EQ(result.mat[1][1], 7.0);
    TEST_ASSERT_EQ(result.mat[1][2], 3.0);
    TEST_ASSERT_EQ(result.mat[2][0], 26.0);
    TEST_ASSERT_EQ(result.mat[2][1], 63.0);
    TEST_ASSERT_EQ(result.mat[2][2], 35.0);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_geom_mat3_minor) {
    GeomMat3 mat = geom_mat3_new(3.0, 2.0, 8.0, 5.0, 1.0, 7.0, 4.0, 9.0, 6.0);

    TEST_ASSERT_EQ(geom_mat3_minor(mat, 0, 0), -57.0);
    TEST_ASSERT_EQ(geom_mat3_minor(mat, 0, 1), 2.0);
    TEST_ASSERT_EQ(geom_mat3_minor(mat, 0, 2), 41.0);
    TEST_ASSERT_EQ(geom_mat3_minor(mat, 1, 0), -60.0);
    TEST_ASSERT_EQ(geom_mat3_minor(mat, 1, 1), -14.0);
    TEST_ASSERT_EQ(geom_mat3_minor(mat, 1, 2), 19.0);
    TEST_ASSERT_EQ(geom_mat3_minor(mat, 2, 0), 6.0);
    TEST_ASSERT_EQ(geom_mat3_minor(mat, 2, 1), -19.0);
    TEST_ASSERT_EQ(geom_mat3_minor(mat, 2, 2), -7.0);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_geom_mat3_det) {
    GeomMat3 mat = geom_mat3_new(3.0, 2.0, 8.0, 5.0, 1.0, 7.0, 4.0, 9.0, 6.0);

    TEST_ASSERT_EQ(geom_mat3_det(mat), 153.0);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_geom_mat3_inverse) {
    GeomMat3 mat = geom_mat3_new(3.0, 2.0, 8.0, 5.0, 1.0, 7.0, 4.0, 9.0, 6.0);
    GeomMat3 inverse = geom_mat3_inverse(mat);

    TEST_ASSERT_EQ(inverse.mat[0][0], -0.37254902);
    TEST_ASSERT_EQ(inverse.mat[0][1], 0.392156863);
    TEST_ASSERT_EQ(inverse.mat[0][2], 0.039215686);
    TEST_ASSERT_EQ(inverse.mat[1][0], -0.013071895);
    TEST_ASSERT_EQ(inverse.mat[1][1], -0.091503268);
    TEST_ASSERT_EQ(inverse.mat[1][2], 0.124183007);
    TEST_ASSERT_EQ(inverse.mat[2][0], 0.267973856);
    TEST_ASSERT_EQ(inverse.mat[2][1], -0.124183007);
    TEST_ASSERT_EQ(inverse.mat[2][2], -0.045751634);

    return TEST_RESULT_PASS;
}

#endif
