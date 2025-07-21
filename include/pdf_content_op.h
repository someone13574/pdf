#pragma once

#include "pdf_object.h"
#include "pdf_types.h"

typedef enum {
    PDF_CONTENT_OP_BEGIN_TEXT, // BT
    PDF_CONTENT_OP_END_TEXT, // ET
    PDF_CONTENT_OP_SET_FONT, // Tf
    PDF_CONTENT_OP_NEXT_LINE, // Td
    PDF_CONTENT_OP_SHOW_TEXT // Tj
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
