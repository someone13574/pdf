#pragma once

#include <stdint.h>

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
    SfntUint16Array* end_pts_of_contours;
    uint16_t instruction_len;
    SfntUint8Array* instructions;
    SfntSimpleGlyphFlagsVec* flags;
    SfntInt16Array* x_coords;
    SfntInt16Array* y_coords;
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

    union {
        SfntSimpleGlyph simple;
        SfntComponentGlyphPart compound;
    } data;
} SfntGlyph;
