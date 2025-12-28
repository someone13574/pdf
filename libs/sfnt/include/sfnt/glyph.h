#pragma once

#include <stdint.h>

#include "arena/common.h"
#include "canvas/canvas.h"
#include "geom/mat3.h"
#include "types.h"

typedef enum {
    SFNT_SIMPLE_GLYPH_ON_CURVE = 1 << 0,
    SFNT_SIMPLE_GLYPH_X_SHORT = 1 << 1,
    SFNT_SIMPLE_GLYPH_Y_SHORT = 1 << 2,
    SFNT_SIMPLE_GLYPH_REPEAT = 1 << 3,
    SFNT_SIMPLE_GLYPH_X_MODIFIER = 1 << 4,
    SFNT_SIMPLE_GLYPH_Y_MODIFIER = 1 << 5
} SfntSimpleGlyphFlagFields;

typedef struct {
    uint8_t flags;
    uint8_t repetitions;
} SfntSimpleGlyphFlags;

#define DVEC_NAME SfntSimpleGlyphFlagsVec
#define DVEC_LOWERCASE_NAME sfnt_simple_glyph_flags_vec
#define DVEC_TYPE SfntSimpleGlyphFlags
#include "arena/dvec_decl.h"

typedef struct {
    bool on_curve;
    int16_t delta_x;
    int16_t delta_y;
} SfntGlyphPoint;

#define DARRAY_NAME SfntGlyphPointArray
#define DARRAY_LOWERCASE_NAME sfnt_glyph_point_array
#define DARRAY_TYPE SfntGlyphPoint
#include "arena/darray_decl.h"

typedef struct {
    Uint16Array* end_pts_of_contours;
    uint16_t instruction_len;
    Uint8Array* instructions;
    SfntSimpleGlyphFlagsVec* flags;
    SfntGlyphPointArray* points;
} SfntSimpleGlyph;

typedef struct {
    uint16_t flags;
    uint16_t glyph_idx;
    int32_t argument1;
    int32_t argument2;
} SfntComponentGlyphPart;

typedef enum {
    SFNT_GLYPH_TYPE_SIMPLE,
    SFNT_GLYPH_TYPE_COMPOUND,
    SFNT_GLYPH_TYPE_NONE
} SfntGlyphType;

typedef struct {
    int16_t num_contours;
    SfntFWord x_min;
    SfntFWord y_min;
    SfntFWord x_max;
    SfntFWord y_max;
    SfntGlyphType glyph_type;

    uint16_t advance_width;
    int16_t left_side_bearing;

    union {
        SfntSimpleGlyph simple;
        SfntComponentGlyphPart compound;
    } data;
} SfntGlyph;

void sfnt_glyph_render(
    Canvas* canvas,
    const SfntGlyph* glyph,
    GeomMat3 transform,
    uint32_t color_rgba
);
