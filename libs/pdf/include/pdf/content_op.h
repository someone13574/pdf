#pragma once

#include "pdf/object.h"
typedef enum {
    PDF_CONTENT_OP_SAVE_GSTATE,     // q
    PDF_CONTENT_OP_RESTORE_GSTATE,  // Q
    PDF_CONTENT_OP_SET_CTM,         // cm
    PDF_CONTENT_OP_BEGIN_TEXT,      // BT
    PDF_CONTENT_OP_END_TEXT,        // ET
    PDF_CONTENT_OP_SET_FONT,        // Tf
    PDF_CONTENT_OP_SET_TEXT_MATRIX, // Tm
    PDF_CONTENT_OP_SHOW_TEXT,       // TJ
    PDF_CONTENT_OP_POSITION_TEXT,   // TJ
    PDF_CONTENT_OP_SET_GRAY,        // g
} PdfContentOpKind;

typedef struct {
    PdfNumber a;
    PdfNumber b;
    PdfNumber c;
    PdfNumber d;
    PdfNumber e;
    PdfNumber f;
} PdfMatrix;

typedef struct {
    PdfName font;
    PdfNumber size;
} PdfOpParamsSetFont;

typedef struct {
    bool stroking;
    PdfNumber gray;
} PdfOpParamsSetGray;

typedef struct {
    PdfContentOpKind kind;

    union {
        PdfMatrix set_ctm;
        PdfOpParamsSetFont set_font;
        PdfMatrix set_text_matrix;
        PdfString show_text;
        PdfNumber position_text;
        PdfOpParamsSetGray set_gray;
    } data;
} PdfContentOp;

#define DVEC_NAME PdfContentOpVec
#define DVEC_LOWERCASE_NAME pdf_content_op_vec
#define DVEC_TYPE PdfContentOp
#include "arena/dvec_decl.h"
