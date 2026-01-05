#include "color/cie_xyz.h"

#include "geom/vec3.h"

CieXYZ cie_xyz_new(double x, double y, double z) {
    return (CieXYZ) {.x = x, .y = y, .z = z};
}

CieXYZ cie_xyz_from_geom(GeomVec3 vec) {
    return cie_xyz_new(vec.x, vec.y, vec.z);
}

GeomVec3 cie_xyz_to_geom(CieXYZ cie_xyz) {
    return geom_vec3_new(cie_xyz.x, cie_xyz.y, cie_xyz.z);
}
