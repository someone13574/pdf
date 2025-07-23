#include "op.h"

#include "../deserialize.h"
#include "arena/arena.h"
#include "log.h"
#include "pdf/content_op.h"
#include "pdf/object.h"
#include "pdf/result.h"
#include "pdf/types.h"

#define DVEC_NAME PdfContentOpVec
#define DVEC_LOWERCASE_NAME pdf_content_op_vec
#define DVEC_TYPE PdfContentOp
#include "../arena/dvec_impl.h"

PdfContentOp*
new_queue_op(PdfContentOpVec* operation_queue, PdfContentOpKind op) {
    return pdf_content_op_vec_push(
        operation_queue,
        (PdfContentOp) {.kind = op}
    );
}

PdfResult pdf_deserialize_set_font_op(
    PdfObjectVec* operands,
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

PdfResult pdf_deserialize_next_line_op(
    PdfObjectVec* operands,
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

PdfResult pdf_deserialize_show_text_op(
    PdfObjectVec* operands,
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

PdfResult pdf_deserialize_content_op(
    PdfOperator op,
    PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpVec* operation_queue
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(operation_queue);

    switch (op) {
        case PDF_OPERATOR_BT: {
            new_queue_op(operation_queue, PDF_CONTENT_OP_BEGIN_TEXT);
            return PDF_OK;
        };
        case PDF_OPERATOR_ET: {
            new_queue_op(operation_queue, PDF_CONTENT_OP_END_TEXT);
            return PDF_OK;
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
        case PDF_OPERATOR_Tj: {
            PdfContentOp* new_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_SHOW_TEXT);
            return pdf_deserialize_show_text_op(
                operands,
                arena,
                &new_op->data.show_text
            );
        }
        default: {
            LOG_TODO("Unimplemented content stream operation: %d", op);
        }
    }
}
