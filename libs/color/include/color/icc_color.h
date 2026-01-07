#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "geom/mat3.h"
#include "geom/vec3.h"

typedef enum {
    ICC_COLOR_SPACE_XYZ = 0,
    ICC_COLOR_SPACE_LAB = 1,
    ICC_COLOR_SPACE_LUV = 2,
    ICC_COLOR_SPACE_Y_CB_CR = 3,
    ICC_COLOR_SPACE_CIE_YYX = 4,
    ICC_COLOR_SPACE_RGB = 5,
    ICC_COLOR_SPACE_GRAY = 6,
    ICC_COLOR_SPACE_HSV = 7,
    ICC_COLOR_SPACE_HLS = 8,
    ICC_COLOR_SPACE_CMYK = 9,
    ICC_COLOR_SPACE_CMY = 10,
    ICC_COLOR_SPACE_2CLR = 11,
    ICC_COLOR_SPACE_3CLR = 12,
    ICC_COLOR_SPACE_4CLR = 13,
    ICC_COLOR_SPACE_5CLR = 14,
    ICC_COLOR_SPACE_6CLR = 15,
    ICC_COLOR_SPACE_7CLR = 16,
    ICC_COLOR_SPACE_8CLR = 17,
    ICC_COLOR_SPACE_9CLR = 18,
    ICC_COLOR_SPACE_10CLR = 19,
    ICC_COLOR_SPACE_11CLR = 20,
    ICC_COLOR_SPACE_12CLR = 21,
    ICC_COLOR_SPACE_13CLR = 22,
    ICC_COLOR_SPACE_14CLR = 23,
    ICC_COLOR_SPACE_15CLR = 24,
    ICC_COLOR_SPACE_UNKNOWN = 25
} IccColorSpace;

uint32_t icc_color_space_signature(IccColorSpace color_space);
IccColorSpace icc_color_space_from_signature(uint32_t signature);

size_t icc_color_space_channels(IccColorSpace space);

typedef struct {
    IccColorSpace color_space;
    double channels[15];
} IccColor;

void icc_color_clamp(IccColor* color);
void icc_color_norm_pcs(IccColor* color, GeomMat3 matrix);

typedef struct {
    GeomVec3 vec;
    bool is_xyz;
} IccPcsColor;

IccPcsColor icc_pcs_color_to_lab(IccPcsColor color);
IccPcsColor icc_pcs_color_to_xyz(IccPcsColor color);
IccColor icc_pcs_to_color(IccPcsColor color);
