#pragma once

#include "geom/mat3.h"
#include "pdf/object.h"

typedef struct {
    PdfReal character_spacing; // T_c
    PdfReal word_spacing; // T_w
    PdfReal horizontal_spacing; // T_h
    PdfReal leading; // T_l
    // text_font T_f
    // text_font_size T_fs
    // text_mode T_mode
    PdfReal text_rise; // T_rise
    // text_knockout
} TextState;

TextState text_state_default();

typedef struct {
    GeomMat3 text_matrix; // T_m
    GeomMat3 text_line_matrix; // T_lm
} TextObjectState;

TextObjectState text_object_state_default();
