#include "op.h"

#include <stdbool.h>

#include "../deserialize.h"
#include "arena/arena.h"
#include "logger/log.h"
#include "operator.h"
#include "pdf/content_op.h"
#include "pdf/object.h"
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

static PdfError* deserialize_set_matrix(
    PdfMatrix* target,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(target);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            &target->a,
            PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
        ),
        PDF_OPERAND(
            &target->b,
            PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
        ),
        PDF_OPERAND(
            &target->c,
            PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
        ),
        PDF_OPERAND(
            &target->d,
            PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
        ),
        PDF_OPERAND(
            &target->e,
            PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
        ),
        PDF_OPERAND(
            &target->f,
            PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
        )
    };

    PDF_PROPAGATE(pdf_deserialize_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        arena
    ));

    return NULL;
}

static PdfError* deserialize_set_font(
    PdfContentOpVec* operation_queue,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(operation_queue);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfContentOp* queue_op =
        new_queue_op(operation_queue, PDF_CONTENT_OP_SET_FONT);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            &queue_op->data.set_font.font,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_OPERAND(
            &queue_op->data.set_gray.gray,
            PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
        )
    };

    PDF_PROPAGATE(pdf_deserialize_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        arena
    ));

    return NULL;
}

static PdfError* deserialize_positioned_text_op(
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
                new_op->data.show_text = element->data.string;
                break;
            }
            case PDF_OBJECT_TYPE_INTEGER: {
                PdfContentOp* new_op =
                    new_queue_op(operation_queue, PDF_CONTENT_OP_POSITION_TEXT);
                new_op->data.position_text.type = PDF_NUMBER_TYPE_INTEGER;
                new_op->data.position_text.value.integer =
                    element->data.integer;
                break;
            }
            case PDF_OBJECT_TYPE_REAL: {
                PdfContentOp* new_op =
                    new_queue_op(operation_queue, PDF_CONTENT_OP_POSITION_TEXT);
                new_op->data.position_text.type = PDF_NUMBER_TYPE_REAL;
                new_op->data.position_text.value.real = element->data.real;
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

static PdfError* deserialize_set_gray(
    bool stroking,
    PdfContentOpVec* operation_queue,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(operation_queue);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfContentOp* queue_op =
        new_queue_op(operation_queue, PDF_CONTENT_OP_SET_GRAY);
    queue_op->data.set_gray.stroking = stroking;

    PdfOperandDescriptor descriptors[] = {PDF_OPERAND(
        &queue_op->data.set_gray.gray,
        PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
    )};

    PDF_PROPAGATE(pdf_deserialize_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        arena
    ));

    return NULL;
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
        case PDF_OPERATOR_q: {
            new_queue_op(operation_queue, PDF_CONTENT_OP_SAVE_GSTATE);
            return NULL;
        }
        case PDF_OPERATOR_Q: {
            new_queue_op(operation_queue, PDF_CONTENT_OP_RESTORE_GSTATE);
            return NULL;
        }
        case PDF_OPERATOR_cm: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_SET_CTM);
            PDF_PROPAGATE(
                deserialize_set_matrix(&queue_op->data.set_ctm, operands, arena)
            );
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
            PDF_PROPAGATE(deserialize_set_font(operation_queue, operands, arena)
            );
            return NULL;
        }
        case PDF_OPERATOR_Tm: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_CONTENT_OP_SET_TEXT_MATRIX);
            PDF_PROPAGATE(deserialize_set_matrix(
                &queue_op->data.set_text_matrix,
                operands,
                arena
            ));
            return NULL;
        }
        case PDF_OPERATOR_TJ: {
            PDF_PROPAGATE(
                deserialize_positioned_text_op(operands, operation_queue)
            );
            return NULL;
        }
        case PDF_OPERATOR_g: {
            PDF_PROPAGATE(
                deserialize_set_gray(false, operation_queue, operands, arena)
            );
            return NULL;
        }
        default: {
            LOG_TODO(
                "Unimplemented deserialization for content stream operation: %d",
                op
            );
        }
    }
}
