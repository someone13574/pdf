#pragma once

#include "pdf/object.h"

typedef enum {
    PDF_CONTENT_OP_SET_LINE_WIDTH, // w
    PDF_CONTENT_OP_SET_GSTATE,     // gs
    PDF_CONTENT_OP_SAVE_GSTATE,    // q
    PDF_CONTENT_OP_RESTORE_GSTATE, // Q
    PDF_CONTENT_OP_SET_CTM,        // cm
    PDF_CONTENT_OP_DRAW_RECT,      // re
    PDF_CONTENT_OP_FILL,           // f
    PDF_CONTENT_OP_BEGIN_TEXT,     // BT
    PDF_CONTENT_OP_END_TEXT,       // ET
    PDF_CONTENT_OP_SET_FONT,       // Tf
    PDF_CONTENT_OP_NEXT_LINE,      // Td
    PDF_CONTENT_OP_SET_TM,         // Tm
    PDF_CONTENT_OP_SHOW_TEXT,      // Tj, TJ
    PDF_CONTENT_OP_POSITION_TEXT,  // TJ
    PDF_CONTENT_OP_SET_GRAY,       // G, g
    PDF_CONTENT_OP_SET_RGB,        // rg
    PDF_CONTENT_OP_PAINT_XOBJECT   // Do
} PdfContentOpKind;

typedef struct {
    PdfNumber width;
} PdfContentOpSetLineWidth;

typedef struct {
    PdfName name;
} PdfContentOpSetGState;

/// Used by PDF_CONTENT_OP_SET_CTM, PDF_CONTENT_OP_SET_TM
typedef struct {
    PdfNumber a;
    PdfNumber b;
    PdfNumber c;
    PdfNumber d;
    PdfNumber e;
    PdfNumber f;
} PdfContentOpSetMatrix;

typedef struct {
    PdfNumber x;
    PdfNumber y;
    PdfNumber width;
    PdfNumber height;
} PdfContentOpDrawRect;

typedef struct {
    PdfName font;
    PdfNumber size;
} PdfContentOpSetFont;

typedef struct {
    PdfNumber t_x;
    PdfNumber t_y;
} PdfContentOpNextLine;

typedef struct {
    PdfString text;
} PdfContentOpShowText;

typedef struct {
    PdfNumber translation;
} PdfContentOpPositionText;

typedef struct {
    bool stroking;
    PdfNumber gray;
} PdfContentOpSetGray;

typedef struct {
    bool stroking;
    PdfNumber r;
    PdfNumber g;
    PdfNumber b;
} PdfContentOpSetRGB;

typedef struct {
    PdfName xobject;
} PdfContentOpPaintXObject;

typedef struct {
    PdfContentOpKind kind;

    union {
        PdfContentOpSetLineWidth set_line_width;
        PdfContentOpSetGState set_gstate;
        PdfContentOpSetMatrix set_matrix;
        PdfContentOpDrawRect draw_rect;
        PdfContentOpSetFont set_font;
        PdfContentOpNextLine next_line;
        PdfContentOpShowText show_text;
        PdfContentOpPositionText position_text;
        PdfContentOpSetGray set_gray;
        PdfContentOpSetRGB set_rgb;
        PdfContentOpPaintXObject paint_xobject;
    } data;
} PdfContentOp;

#define DVEC_NAME PdfContentOpVec
#define DVEC_LOWERCASE_NAME pdf_content_op_vec
#define DVEC_TYPE PdfContentOp
#include "arena/dvec_decl.h"
