#include "op.h"

#include "../deserialize.h"
#include "arena/arena.h"
#include "logger/log.h"
#include "operator.h"
#include "pdf/content_op.h"
#include "pdf/object.h"
#include "pdf/types.h"
#include "pdf_error/error.h"

#define DVEC_NAME PdfContentOpVec
#define DVEC_LOWERCASE_NAME pdf_content_op_vec
#define DVEC_TYPE PdfContentOp
#include "arena/dvec_impl.h"

static PdfContentOp*
new_queue_op(PdfContentOpVec* operation_queue, PdfContentOpKind op) {
    return pdf_content_op_vec_push(
        operation_queue,
        (PdfContentOp) {.kind = op}
    );
}

static PdfError* pdf_deserialize_set_line_width_op(
    const PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpSetLineWidth* deserialized
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfOperandDescriptor descriptors[] = {PDF_OPERAND(
        PdfContentOpSetLineWidth,
        width,
        PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
    )};
    return pdf_deserialize_operands(
        deserialized,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        operands,
        arena
    );
}

static PdfError* pdf_deserialize_set_gstate_op(
    const PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpSetGState* deserialized
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfOperandDescriptor descriptors[] = {PDF_OPERAND(
        PdfContentOpSetGState,
        name,
        PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
    )};
    return pdf_deserialize_operands(
        deserialized,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        operands,
        arena
    );
}

static PdfError* pdf_deserialize_set_matrix_op(
    const PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpSetMatrix* deserialized
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            PdfContentOpSetMatrix,
            a,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        ),
        PDF_OPERAND(
            PdfContentOpSetMatrix,
            b,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        ),
        PDF_OPERAND(
            PdfContentOpSetMatrix,
            c,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        ),
        PDF_OPERAND(
            PdfContentOpSetMatrix,
            d,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        ),
        PDF_OPERAND(
            PdfContentOpSetMatrix,
            e,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        ),
        PDF_OPERAND(
            PdfContentOpSetMatrix,
            f,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        )
    };
    return pdf_deserialize_operands(
        deserialized,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        operands,
        arena
    );
}

static PdfError* pdf_deserialize_draw_rect_op(
    const PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpDrawRect* deserialized
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            PdfContentOpDrawRect,
            x,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        ),
        PDF_OPERAND(
            PdfContentOpDrawRect,
            y,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        ),
        PDF_OPERAND(
            PdfContentOpDrawRect,
            width,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        ),
        PDF_OPERAND(
            PdfContentOpDrawRect,
            height,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        )
    };
    return pdf_deserialize_operands(
        deserialized,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        operands,
        arena
    );
}

static PdfError* pdf_deserialize_set_font_op(
    const PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpSetFont* deserialized
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            PdfContentOpSetFont,
            font,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_OPERAND(
            PdfContentOpSetFont,
            size,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        )
    };
    return pdf_deserialize_operands(
        deserialized,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        operands,
        arena
    );
}

static PdfError* pdf_deserialize_next_line_op(
    const PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpNextLine* deserialized
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            PdfContentOpNextLine,
            t_x,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        ),
        PDF_OPERAND(
            PdfContentOpNextLine,
            t_y,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        )
    };
    return pdf_deserialize_operands(
        deserialized,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        operands,
        arena
    );
}

static PdfError* pdf_deserialize_single_show_text_op(
    const PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpShowText* deserialized
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfOperandDescriptor descriptors[] = {PDF_OPERAND(
        PdfContentOpShowText,
        text,
        PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STRING)
    )};
    return pdf_deserialize_operands(
        deserialized,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        operands,
        arena
    );
}

static PdfError* pdf_deserialize_show_positioned_text_op(
    const PdfObjectVec* operands,
    PdfContentOpVec* operation_queue
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(operation_queue);

    if (pdf_object_vec_len(operands) < 1) {
        return PDF_ERROR(PDF_ERR_MISSING_OPERAND);
    } else if (pdf_object_vec_len(operands) > 1) {
        return PDF_ERROR(PDF_ERR_EXCESS_OPERAND);
    }

    PdfObject* array_object = NULL;
    RELEASE_ASSERT(pdf_object_vec_get(operands, 0, &array_object));

    if (array_object->type != PDF_OBJECT_TYPE_ARRAY) {
        return PDF_ERROR(PDF_ERR_INCORRECT_TYPE);
    }

    for (size_t idx = 0;
         idx < pdf_object_vec_len(array_object->data.array.elements);
         idx++) {
        PdfObject* element = NULL;
        RELEASE_ASSERT(
            pdf_object_vec_get(array_object->data.array.elements, idx, &element)
        );

        switch (element->type) {
            case PDF_OBJECT_TYPE_STRING: {
                PdfContentOp* new_op =
                    new_queue_op(operation_queue, PDF_CONTENT_OP_SHOW_TEXT);
                new_op->data.show_text.text = element->data.string;
                break;
            }
            case PDF_OBJECT_TYPE_INTEGER: {
                PdfContentOp* new_op =
                    new_queue_op(operation_queue, PDF_CONTENT_OP_POSITION_TEXT);
                new_op->data.position_text.translation.type =
                    PDF_NUMBER_TYPE_INTEGER;
                new_op->data.position_text.translation.value.integer =
                    element->data.integer;
                break;
            }
            case PDF_OBJECT_TYPE_REAL: {
                PdfContentOp* new_op =
                    new_queue_op(operation_queue, PDF_CONTENT_OP_POSITION_TEXT);
                new_op->data.position_text.translation.type =
                    PDF_NUMBER_TYPE_REAL;
                new_op->data.position_text.translation.value.real =
                    element->data.real;
                break;
            }
            default: {
                return PDF_ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "Expected a string or number in positioned text array"
                );
            }
        }
    }

    return NULL;
}

static PdfError* pdf_deserialize_set_gray_op(
    const PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpSetGray* deserialized
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfOperandDescriptor descriptors[] = {PDF_OPERAND(
        PdfContentOpSetGray,
        gray,
        PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
    )};
    return pdf_deserialize_operands(
        deserialized,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        operands,
        arena
    );
}

static PdfError* pdf_deserialize_set_rgb_op(
    const PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpSetRGB* deserialized
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            PdfContentOpSetRGB,
            r,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        ),
        PDF_OPERAND(
            PdfContentOpSetRGB,
            g,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        ),
        PDF_OPERAND(
            PdfContentOpSetRGB,
            b,
            PDF_CUSTOM_FIELD(pdf_deserialize_number_wrapper)
        )
    };
    return pdf_deserialize_operands(
        deserialized,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        operands,
        arena
    );
}

static PdfError* pdf_deserialize_paint_xobject_op(
    const PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpPaintXObject* deserialized
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfOperandDescriptor descriptors[] = {PDF_OPERAND(
        PdfContentOpPaintXObject,
        xobject,
        PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
    )};
    return pdf_deserialize_operands(
        deserialized,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        operands,
        arena
    );
}

PdfError* pdf_deserialize_content_op(
    PdfOperator op,
    const PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpVec* operation_queue
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(operation_queue);

    switch (op) {
        case PDF_OPERATOR_w: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_SET_LINE_WIDTH);
            return pdf_deserialize_set_line_width_op(
                operands,
                arena,
                &new_op->data.set_line_width
            );
        }
        case PDF_OPERATOR_gs: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_SET_GSTATE);
            return pdf_deserialize_set_gstate_op(
                operands,
                arena,
                &new_op->data.set_gstate
            );
        }
        case PDF_OPERATOR_q: {
            new_queue_op(operation_queue, PDF_CONTENT_OP_SAVE_GSTATE);
            return NULL;
        }
        case PDF_OPERATOR_Q: {
            new_queue_op(operation_queue, PDF_CONTENT_OP_RESTORE_GSTATE);
            return NULL;
        }
        case PDF_OPERATOR_cm: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_SET_CTM);
            return pdf_deserialize_set_matrix_op(
                operands,
                arena,
                &new_op->data.set_matrix
            );
        }
        case PDF_OPERATOR_re: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_DRAW_RECT);
            return pdf_deserialize_draw_rect_op(
                operands,
                arena,
                &new_op->data.draw_rect
            );
        }
        case PDF_OPERATOR_f: {
            new_queue_op(operation_queue, PDF_CONTENT_OP_FILL);
            return NULL;
        }
        case PDF_OPERATOR_BT: {
            new_queue_op(operation_queue, PDF_CONTENT_OP_BEGIN_TEXT);
            return NULL;
        };
        case PDF_OPERATOR_ET: {
            new_queue_op(operation_queue, PDF_CONTENT_OP_END_TEXT);
            return NULL;
        };
        case PDF_OPERATOR_Tf: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_SET_FONT);
            return pdf_deserialize_set_font_op(
                operands,
                arena,
                &new_op->data.set_font
            );
        }
        case PDF_OPERATOR_Td: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_NEXT_LINE);
            return pdf_deserialize_next_line_op(
                operands,
                arena,
                &new_op->data.next_line
            );
        }
        case PDF_OPERATOR_Tm: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_SET_TM);
            return pdf_deserialize_set_matrix_op(
                operands,
                arena,
                &new_op->data.set_matrix
            );
        }
        case PDF_OPERATOR_Tj: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_SHOW_TEXT);
            return pdf_deserialize_single_show_text_op(
                operands,
                arena,
                &new_op->data.show_text
            );
        }
        case PDF_OPERATOR_TJ: {
            return pdf_deserialize_show_positioned_text_op(
                operands,
                operation_queue
            );
        }
        case PDF_OPERATOR_g: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_SET_GRAY);
            new_op->data.set_gray.stroking = false;
            return pdf_deserialize_set_gray_op(
                operands,
                arena,
                &new_op->data.set_gray
            );
        }
        case PDF_OPERATOR_RG: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_SET_RGB);
            new_op->data.set_rgb.stroking = true;
            return pdf_deserialize_set_rgb_op(
                operands,
                arena,
                &new_op->data.set_rgb
            );
        }
        case PDF_OPERATOR_rg: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_SET_RGB);
            new_op->data.set_rgb.stroking = false;
            return pdf_deserialize_set_rgb_op(
                operands,
                arena,
                &new_op->data.set_rgb
            );
        }
        case PDF_OPERATOR_Do: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_PAINT_XOBJECT);
            return pdf_deserialize_paint_xobject_op(
                operands,
                arena,
                &new_op->data.paint_xobject
            );
        }
        default: {
            LOG_TODO("Unimplemented content stream operation: %d", op);
        }
    }
}
