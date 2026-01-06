#include "color/cie.h"

#include <math.h>

#include "geom/vec3.h"

CieXYZ cie_xyz_new(double x, double y, double z) {
    return (CieXYZ) {.x = x, .y = y, .z = z};
}

CieLab cie_lab_new(double l, double a, double b) {
    return (CieLab) {.l = l, .a = a, .b = b};
}

CieXYZ cie_xyz_from_geom(GeomVec3 vec) {
    return cie_xyz_new(vec.x, vec.y, vec.z);
}

GeomVec3 cie_xyz_to_geom(CieXYZ cie_xyz) {
    return geom_vec3_new(cie_xyz.x, cie_xyz.y, cie_xyz.z);
}

CieLab cie_lab_from_geom(GeomVec3 vec) {
    return cie_lab_new(vec.x, vec.y, vec.z);
}

GeomVec3 cie_lab_to_geom(CieLab cie_lab) {
    return geom_vec3_new(cie_lab.l, cie_lab.a, cie_lab.b);
}

static double cie_xyz_to_cie_lab_f(double t) {
    const double DELTA = 6.0 / 29.0;
    if (t > DELTA * DELTA * DELTA) {
        return pow(t, 1.0 / 3.0);
    } else {
        return t / DELTA / DELTA / 3.0 + 4.0 / 29.0;
    }
}

CieLab cie_xyz_to_cie_lab(CieXYZ cie_xyz, CieXYZ reference_illuminant) {
    double fx = cie_xyz_to_cie_lab_f(cie_xyz.x / reference_illuminant.x);
    double fy = cie_xyz_to_cie_lab_f(cie_xyz.y / reference_illuminant.y);
    double fz = cie_xyz_to_cie_lab_f(cie_xyz.z / reference_illuminant.z);

    return cie_lab_new(116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz));
}

static double cie_xyz_to_cie_lab_f_inv(double t) {
    const double DELTA = 6.0 / 29.0;
    if (t > DELTA * DELTA * DELTA) {
        return pow(t, 3.0);
    } else {
        return 3.0 * DELTA * DELTA * (t - 4.0 / 29.0);
    }
}

CieXYZ cie_lab_to_cie_xyz(CieLab cie_lab, CieXYZ reference_illuminant) {
    return cie_xyz_new(
        reference_illuminant.x
            * cie_xyz_to_cie_lab_f_inv(
                (cie_lab.l + 16.0) / 116.0 + cie_lab.a / 500.0
            ),
        reference_illuminant.y
            * cie_xyz_to_cie_lab_f_inv((cie_lab.l + 16.0) / 116.0),
        reference_illuminant.z
            * cie_xyz_to_cie_lab_f_inv(
                (cie_lab.l + 16.0) / 116.0 - cie_lab.b / 200.0
            )
    );
}
