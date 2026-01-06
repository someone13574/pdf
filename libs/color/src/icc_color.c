#include "color/icc_color.h"

#include "color/cie.h"
#include "geom/mat3.h"
#include "geom/vec3.h"
#include "logger/log.h"

uint32_t icc_color_space_signature(ICCColorSpace color_space) {
    switch (color_space) {
        case ICC_COLOR_SPACE_XYZ: {
            return 0x58595A20;
        }
        case ICC_COLOR_SPACE_LAB: {
            return 0x4C616220;
        }
        case ICC_COLOR_SPACE_LUV: {
            return 0x4C757620;
        }
        case ICC_COLOR_SPACE_Y_CB_CR: {
            return 0x59436272;
        }
        case ICC_COLOR_SPACE_CIE_YYX: {
            return 0x59787920;
        }
        case ICC_COLOR_SPACE_RGB: {
            return 0x52474220;
        }
        case ICC_COLOR_SPACE_GRAY: {
            return 0x47524159;
        }
        case ICC_COLOR_SPACE_HSV: {
            return 0x48535620;
        }
        case ICC_COLOR_SPACE_HLS: {
            return 0x484C5320;
        }
        case ICC_COLOR_SPACE_CMYK: {
            return 0x434D594B;
        }
        case ICC_COLOR_SPACE_CMY: {
            return 0x434D5920;
        }
        case ICC_COLOR_SPACE_2CLR: {
            return 0x32434C52;
        }
        case ICC_COLOR_SPACE_3CLR: {
            return 0x33434C52;
        }
        case ICC_COLOR_SPACE_4CLR: {
            return 0x34434C52;
        }
        case ICC_COLOR_SPACE_5CLR: {
            return 0x35434C52;
        }
        case ICC_COLOR_SPACE_6CLR: {
            return 0x36434C52;
        }
        case ICC_COLOR_SPACE_7CLR: {
            return 0x37434C52;
        }
        case ICC_COLOR_SPACE_8CLR: {
            return 0x38434C52;
        }
        case ICC_COLOR_SPACE_9CLR: {
            return 0x39434C52;
        }
        case ICC_COLOR_SPACE_10CLR: {
            return 0x41434C52;
        }
        case ICC_COLOR_SPACE_11CLR: {
            return 0x42434C52;
        }
        case ICC_COLOR_SPACE_12CLR: {
            return 0x43434C52;
        }
        case ICC_COLOR_SPACE_13CLR: {
            return 0x44434C52;
        }
        case ICC_COLOR_SPACE_14CLR: {
            return 0x45434C52;
        }
        case ICC_COLOR_SPACE_15CLR: {
            return 0x46434C52;
        }
        case ICC_COLOR_SPACE_UNKNOWN: {
            return 0xffffffff;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }
}

ICCColorSpace icc_color_space_from_signature(uint32_t signature) {
    switch (signature) {
        case 0x58595A20: {
            return ICC_COLOR_SPACE_XYZ;
        }
        case 0x4C616220: {
            return ICC_COLOR_SPACE_LAB;
        }
        case 0x4C757620: {
            return ICC_COLOR_SPACE_LUV;
        }
        case 0x59436272: {
            return ICC_COLOR_SPACE_Y_CB_CR;
        }
        case 0x59787920: {
            return ICC_COLOR_SPACE_CIE_YYX;
        }
        case 0x52474220: {
            return ICC_COLOR_SPACE_RGB;
        }
        case 0x47524159: {
            return ICC_COLOR_SPACE_GRAY;
        }
        case 0x48535620: {
            return ICC_COLOR_SPACE_HSV;
        }
        case 0x484C5320: {
            return ICC_COLOR_SPACE_HLS;
        }
        case 0x434D594B: {
            return ICC_COLOR_SPACE_CMYK;
        }
        case 0x434D5920: {
            return ICC_COLOR_SPACE_CMY;
        }
        case 0x32434C52: {
            return ICC_COLOR_SPACE_2CLR;
        }
        case 0x33434C52: {
            return ICC_COLOR_SPACE_3CLR;
        }
        case 0x34434C52: {
            return ICC_COLOR_SPACE_4CLR;
        }
        case 0x35434C52: {
            return ICC_COLOR_SPACE_5CLR;
        }
        case 0x36434C52: {
            return ICC_COLOR_SPACE_6CLR;
        }
        case 0x37434C52: {
            return ICC_COLOR_SPACE_7CLR;
        }
        case 0x38434C52: {
            return ICC_COLOR_SPACE_8CLR;
        }
        case 0x39434C52: {
            return ICC_COLOR_SPACE_9CLR;
        }
        case 0x41434C52: {
            return ICC_COLOR_SPACE_10CLR;
        }
        case 0x42434C52: {
            return ICC_COLOR_SPACE_11CLR;
        }
        case 0x43434C52: {
            return ICC_COLOR_SPACE_12CLR;
        }
        case 0x44434C52: {
            return ICC_COLOR_SPACE_13CLR;
        }
        case 0x45434C52: {
            return ICC_COLOR_SPACE_14CLR;
        }
        case 0x46434C52: {
            return ICC_COLOR_SPACE_15CLR;
        }
        default: {
            return ICC_COLOR_SPACE_UNKNOWN;
        }
    }
}

size_t icc_color_space_channels(ICCColorSpace space) {
    switch (space) {
        case ICC_COLOR_SPACE_GRAY: {
            return 1;
        }
        case ICC_COLOR_SPACE_2CLR: {
            return 2;
        }
        case ICC_COLOR_SPACE_XYZ:
        case ICC_COLOR_SPACE_LAB:
        case ICC_COLOR_SPACE_LUV:
        case ICC_COLOR_SPACE_Y_CB_CR:
        case ICC_COLOR_SPACE_CIE_YYX:
        case ICC_COLOR_SPACE_RGB:
        case ICC_COLOR_SPACE_HSV:
        case ICC_COLOR_SPACE_HLS:
        case ICC_COLOR_SPACE_CMY:
        case ICC_COLOR_SPACE_3CLR: {
            return 3;
        }

        case ICC_COLOR_SPACE_CMYK:
        case ICC_COLOR_SPACE_4CLR: {
            return 4;
        }
        case ICC_COLOR_SPACE_5CLR: {
            return 5;
        }
        case ICC_COLOR_SPACE_6CLR: {
            return 6;
        }
        case ICC_COLOR_SPACE_7CLR: {
            return 7;
        }
        case ICC_COLOR_SPACE_8CLR: {
            return 8;
        }
        case ICC_COLOR_SPACE_9CLR: {
            return 9;
        }
        case ICC_COLOR_SPACE_10CLR: {
            return 10;
        }
        case ICC_COLOR_SPACE_11CLR: {
            return 11;
        }
        case ICC_COLOR_SPACE_12CLR: {
            return 12;
        }
        case ICC_COLOR_SPACE_13CLR: {
            return 13;
        }
        case ICC_COLOR_SPACE_14CLR: {
            return 14;
        }
        case ICC_COLOR_SPACE_15CLR: {
            return 15;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }
}

void icc_color_clamp(ICCColor* color) {
    RELEASE_ASSERT(color);

    for (size_t idx = 0; idx < icc_color_space_channels(color->color_space);
         idx++) {
        double* c = &color->channels[idx];
        *c = *c < 0.0 ? 0.0 : *c;
        *c = *c > 1.0 ? 1.0 : *c;
    }
}

void icc_color_norm_pcs(ICCColor* color, GeomMat3 matrix) {
    if (color->color_space == icc_color_space_signature(ICC_COLOR_SPACE_XYZ)) {
        GeomVec3 xyz = geom_vec3_transform(
            geom_vec3_new(
                color->channels[0],
                color->channels[1],
                color->channels[2]
            ),
            matrix
        );
        color->channels[0] = xyz.x;
        color->channels[1] = xyz.y;
        color->channels[2] = xyz.z;
    }

    icc_color_clamp(color);
}

static CieXYZ D50 = {.x = 0.9642, .y = 1.0, .z = 0.8249};

ICCPcsColor icc_pcs_color_to_lab(ICCPcsColor color) {
    if (color.is_xyz) {
        return (ICCPcsColor) {
            .vec = cie_lab_to_geom(
                cie_xyz_to_cie_lab(cie_xyz_from_geom(color.vec), D50)
            ),
            .is_xyz = false
        };
    } else {
        return color;
    }
}

ICCPcsColor icc_pcs_color_to_xyz(ICCPcsColor color) {
    if (!color.is_xyz) {
        return (ICCPcsColor) {
            .vec = cie_xyz_to_geom(
                cie_lab_to_cie_xyz(cie_lab_from_geom(color.vec), D50)
            ),
            .is_xyz = false
        };
    } else {
        return color;
    }
}

ICCColor icc_pcs_to_color(ICCPcsColor color) {
    return (ICCColor) {
        .channels = {color.vec.x, color.vec.y, color.vec.z},
        .color_space = color.is_xyz ? ICC_COLOR_SPACE_XYZ : ICC_COLOR_SPACE_LAB
    };
}
