#pragma once

#include "pdf/object.h"
#include "pdf/types.h"

typedef enum {
    PDF_CONTENT_OP_BEGIN_TEXT, // BT
    PDF_CONTENT_OP_END_TEXT,   // ET
    PDF_CONTENT_OP_SET_FONT,   // Tf
    PDF_CONTENT_OP_NEXT_LINE,  // Td
    PDF_CONTENT_OP_SHOW_TEXT   // Tj
} PdfContentOpKind;

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
    PdfContentOpKind kind;

    union {
        PdfContentOpSetFont set_font;
        PdfContentOpNextLine next_line;
        PdfContentOpShowText show_text;
    } data;
} PdfContentOp;

#define DVEC_NAME PdfContentOpVec
#define DVEC_LOWERCASE_NAME pdf_content_op_vec
#define DVEC_TYPE PdfContentOp
#include "arena/dvec_decl.h"
