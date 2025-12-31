#pragma once

#include <stdbool.h>

#include "geom/vec2.h"
#include "pdf/content_stream/operator.h"
#include "pdf/object.h"
#include "pdf/resources.h"

typedef struct {
    PdfName font;
    PdfReal size;
} PdfOpParamsSetFont;

typedef struct {
    enum { POSITIONED_TEXT_ELEMENT_STR, POSITIONED_TEXT_ELEMENT_OFFSET } type;

    union {
        PdfString str;
        PdfReal offset;
    } value;
} PdfOpParamsPositionedTextElement;

#define DVEC_NAME PdfOpParamsPositionedTextVec
#define DVEC_LOWERCASE_NAME pdf_op_params_positioned_text_vec
#define DVEC_TYPE PdfOpParamsPositionedTextElement
#include "arena/dvec_decl.h"

typedef struct {
    PdfReal r;
    PdfReal g;
    PdfReal b;
} PdfOpParamsSetRGB;

typedef struct {
    PdfReal c;
    PdfReal m;
    PdfReal y;
    PdfReal k;
} PdfOpParamsSetCMYK;

typedef struct {
    GeomVec2 c1;
    GeomVec2 c2;
    GeomVec2 end;
} PdfOpParamsCubicBezier;

typedef struct {
    GeomVec2 a;
    GeomVec2 b;
} PdfOpParamsPartCubicBezier;

typedef struct {
    PdfOperator kind;

    union {
        PdfReal set_line_width;
        PdfLineCapStyle set_line_cap;
        PdfLineJoinStyle set_join_style;
        PdfReal miter_limit;
        PdfReal flatness;
        PdfName set_gstate;
        PdfName set_color_space;
        PdfObjectVec* set_color;
        GeomMat3 set_ctm;
        GeomVec2 new_subpath;
        GeomVec2 line_to;
        PdfOpParamsCubicBezier cubic_bezier;
        PdfOpParamsPartCubicBezier part_cubic_bezier;
        PdfReal set_text_metric;
        PdfOpParamsSetFont set_font;
        GeomVec2 text_offset;
        GeomMat3 set_text_matrix;
        PdfOpParamsPositionedTextVec* positioned_text;
        PdfReal set_gray;
        PdfOpParamsSetRGB set_rgb;
        PdfOpParamsSetCMYK set_cmyk;
        PdfName paint_xobject;
    } data;
} PdfContentOp;

#define DVEC_NAME PdfContentOpVec
#define DVEC_LOWERCASE_NAME pdf_content_op_vec
#define DVEC_TYPE PdfContentOp
#include "arena/dvec_decl.h"
