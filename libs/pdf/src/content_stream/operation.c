#include "pdf/content_stream/operation.h"

#include <stdbool.h>

#include "../deserialize.h"
#include "arena/arena.h"
#include "geom/mat3.h"
#include "logger/log.h"
#include "operation.h"
#include "pdf/content_stream/operator.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

#define DVEC_NAME PdfContentOpVec
#define DVEC_LOWERCASE_NAME pdf_content_op_vec
#define DVEC_TYPE PdfContentOp
#include "arena/dvec_impl.h"

#define DVEC_NAME PdfOpParamsPositionedTextVec
#define DVEC_LOWERCASE_NAME pdf_op_params_positioned_text_vec
#define DVEC_TYPE PdfOpParamsPositionedTextElement
#include "arena/dvec_impl.h"

static PdfContentOp*
new_queue_op(PdfContentOpVec* operation_queue, PdfOperator op) {
    return pdf_content_op_vec_push(
        operation_queue,
        (PdfContentOp) {.kind = op}
    );
}

static PdfError* deserialize_line_cap_style(
    PdfLineCapStyle* target_ptr,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfInteger type;
    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(&type, PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER))
    };

    PDF_PROPAGATE(pdf_deserialize_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        arena
    ));

    switch (type) {
        case 0: {
            *target_ptr = PDF_LINE_CAP_STYLE_BUTT;
            break;
        }
        case 1: {
            *target_ptr = PDF_LINE_CAP_STYLE_ROUND;
            break;
        }
        case 2: {
            *target_ptr = PDF_LINE_CAP_STYLE_PROJECTING;
            break;
        }
        default: {
            return PDF_ERROR(
                PDF_ERR_INVALID_NUMBER,
                "Line cap style must be in range 0-2 inclusive"
            );
        }
    }

    return NULL;
}

static PdfError* deserialize_line_join_style(
    PdfLineJoinStyle* target_ptr,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfInteger type;
    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(&type, PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER))
    };

    PDF_PROPAGATE(pdf_deserialize_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        arena
    ));

    switch (type) {
        case 0: {
            *target_ptr = PDF_LINE_JOIN_STYLE_MITER;
            break;
        }
        case 1: {
            *target_ptr = PDF_LINE_JOIN_STYLE_ROUND;
            break;
        }
        case 2: {
            *target_ptr = PDF_LINE_JOIN_STYLE_BEVEL;
            break;
        }
        default: {
            return PDF_ERROR(
                PDF_ERR_INVALID_NUMBER,
                "Line join style must be in range 0-2 inclusive"
            );
        }
    }

    return NULL;
}

static PdfError* deserialize_matrix(
    GeomMat3* target,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(target);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfReal a, b, c, d, e, f;
    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            &a,
            PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &b,
            PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &c,
            PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &d,
            PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &e,
            PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &f,
            PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
        )
    };

    PDF_PROPAGATE(pdf_deserialize_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        arena
    ));

    *target = geom_mat3_new_pdf(a, b, c, d, e, f);

    return NULL;
}

static PdfError* deserialize_draw_rectangle(
    PdfContentOpVec* operation_queue,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(operation_queue);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfNumber x, y, width, height;

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(&x, PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)),
        PDF_OPERAND(&y, PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)),
        PDF_OPERAND(
            &width,
            PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
        ),
        PDF_OPERAND(
            &height,
            PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
        )
    };

    PDF_PROPAGATE(pdf_deserialize_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        arena
    ));

    double x_real = pdf_number_as_real(x);
    double y_real = pdf_number_as_real(y);
    double width_real = pdf_number_as_real(width);
    double height_real = pdf_number_as_real(height);

    PdfContentOp* new_subpath_op =
        new_queue_op(operation_queue, PDF_OPERATOR_m);
    PdfContentOp* line_op_a = new_queue_op(operation_queue, PDF_OPERATOR_l);
    PdfContentOp* line_op_b = new_queue_op(operation_queue, PDF_OPERATOR_l);
    PdfContentOp* line_op_c = new_queue_op(operation_queue, PDF_OPERATOR_l);
    new_queue_op(operation_queue, PDF_OPERATOR_h);

    new_subpath_op->data.new_subpath.x = x_real;
    new_subpath_op->data.new_subpath.y = y_real;
    line_op_a->data.line_to.x = x_real + width_real;
    line_op_a->data.line_to.y = y_real;
    line_op_b->data.line_to.x = x_real + width_real;
    line_op_b->data.line_to.y = y_real + height_real;
    line_op_c->data.line_to.x = x_real;
    line_op_c->data.line_to.y = y_real + height_real;

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

    PdfContentOp* queue_op = new_queue_op(operation_queue, PDF_OPERATOR_Tf);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            &queue_op->data.set_font.font,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_OPERAND(
            &queue_op->data.set_font.size,
            PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
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

static PdfError* deserialize_vec2(
    GeomVec2* target_ptr,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            &target_ptr->x,
            PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &target_ptr->y,
            PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
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

static PdfError* deserialize_text_op(
    PdfOpParamsPositionedTextVec** target_vec,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(target_vec);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfString string;
    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(&string, PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_STRING))
    };
    PDF_PROPAGATE(pdf_deserialize_operands(operands, descriptors, 1, arena));

    PdfOpParamsPositionedTextElement* element =
        pdf_op_params_positioned_text_vec_push_uninit((void*)target_vec, arena);
    element->type = POSITIONED_TEXT_ELEMENT_STR;
    element->value.str = string;

    return NULL;
}

static PdfError* deserialize_positioned_text_element(
    const PdfObject* object,
    PdfOpParamsPositionedTextElement* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(arena);

    switch (object->type) {
        case PDF_OBJECT_TYPE_INTEGER: {
            target_ptr->type = POSITIONED_TEXT_ELEMENT_OFFSET;
            target_ptr->value.offset = (PdfReal)object->data.integer;
            break;
        }
        case PDF_OBJECT_TYPE_REAL: {
            target_ptr->type = POSITIONED_TEXT_ELEMENT_OFFSET;
            target_ptr->value.offset = object->data.real;
            break;
        }
        case PDF_OBJECT_TYPE_STRING: {
            target_ptr->type = POSITIONED_TEXT_ELEMENT_STR;
            target_ptr->value.str = object->data.string;
            break;
        }
        default: {
            return PDF_ERROR(
                PDF_ERR_INCORRECT_TYPE,
                "Expected a string or number, found type %d",
                (int)object->type
            );
        }
    }

    return NULL;
}

DESERDE_IMPL_TRAMPOLINE(
    deserialize_positioned_text_element_trampoline,
    deserialize_positioned_text_element
)

static PdfError* deserialize_positioned_text_op(
    PdfOpParamsPositionedTextVec** target_vec,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(target_vec);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfOperandDescriptor descriptors[] = {PDF_OPERAND(
        target_vec,
        PDF_DESERDE_ARRAY(
            pdf_op_params_positioned_text_vec_push_uninit,
            PDF_DESERDE_CUSTOM(deserialize_positioned_text_element_trampoline)
        )
    )};

    PDF_PROPAGATE(pdf_deserialize_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        arena
    ));

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

    PdfContentOp* queue_op = new_queue_op(
        operation_queue,
        stroking ? PDF_OPERATOR_g : PDF_OPERATOR_G
    );

    PdfOperandDescriptor descriptors[] = {PDF_OPERAND(
        &queue_op->data.set_gray,
        PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
    )};

    PDF_PROPAGATE(pdf_deserialize_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        arena
    ));

    return NULL;
}

static PdfError* deserialize_set_rgb(
    bool stroking,
    PdfContentOpVec* operation_queue,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(operation_queue);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfContentOp* queue_op = new_queue_op(
        operation_queue,
        stroking ? PDF_OPERATOR_RG : PDF_OPERATOR_rg
    );

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            &queue_op->data.set_rgb.r,
            PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &queue_op->data.set_rgb.g,
            PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &queue_op->data.set_rgb.b,
            PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
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

static PdfError* deserialize_num_real(
    PdfReal* target,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(target);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfOperandDescriptor descriptors[] = {PDF_OPERAND(
        target,
        PDF_DESERDE_CUSTOM(pdf_deserialize_num_as_real_trampoline)
    )};
    PDF_PROPAGATE(pdf_deserialize_operands(operands, descriptors, 1, arena));

    return NULL;
}

static PdfError*
deserialize_name(PdfName* target, const PdfObjectVec* operands, Arena* arena) {
    RELEASE_ASSERT(target);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(target, PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME))
    };
    PDF_PROPAGATE(pdf_deserialize_operands(operands, descriptors, 1, arena));

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

    // TODO: For zero-operand operators, check that they have zero operands.
    switch (op) {
        case PDF_OPERATOR_w: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_w);
            PDF_PROPAGATE(deserialize_num_real(
                &queue_op->data.set_line_width,
                operands,
                arena
            ));
            return NULL;
        }
        case PDF_OPERATOR_J: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_J);
            PDF_PROPAGATE(deserialize_line_cap_style(
                &queue_op->data.set_line_cap,
                operands,
                arena
            ));
            return NULL;
        }
        case PDF_OPERATOR_j: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_J);
            PDF_PROPAGATE(deserialize_line_join_style(
                &queue_op->data.set_join_style,
                operands,
                arena
            ));
            return NULL;
        }
        case PDF_OPERATOR_M: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_M);
            PDF_PROPAGATE(deserialize_num_real(
                &queue_op->data.miter_limit,
                operands,
                arena
            ));
            return NULL;
        }
        case PDF_OPERATOR_d: {
            LOG_WARN(RENDER, "TODO: Render dashed lines");
            return NULL;
        }
        case PDF_OPERATOR_gs: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_gs);
            PDF_PROPAGATE(
                deserialize_name(&queue_op->data.set_gstate, operands, arena)
            );
            return NULL;
        }
        case PDF_OPERATOR_q: {
            new_queue_op(operation_queue, PDF_OPERATOR_q);
            return NULL;
        }
        case PDF_OPERATOR_Q: {
            new_queue_op(operation_queue, PDF_OPERATOR_Q);
            return NULL;
        }
        case PDF_OPERATOR_cm: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_cm);
            PDF_PROPAGATE(
                deserialize_matrix(&queue_op->data.set_ctm, operands, arena)
            );
            return NULL;
        }
        case PDF_OPERATOR_re: {
            PDF_PROPAGATE(
                deserialize_draw_rectangle(operation_queue, operands, arena)
            );
            return NULL;
        }
        case PDF_OPERATOR_f: {
            new_queue_op(operation_queue, PDF_OPERATOR_f);
            return NULL;
        }
        case PDF_OPERATOR_B: {
            new_queue_op(operation_queue, PDF_OPERATOR_B);
            return NULL;
        }
        case PDF_OPERATOR_BT: {
            new_queue_op(operation_queue, PDF_OPERATOR_BT);
            return NULL;
        };
        case PDF_OPERATOR_ET: {
            new_queue_op(operation_queue, PDF_OPERATOR_ET);
            return NULL;
        };
        case PDF_OPERATOR_Tf: {
            PDF_PROPAGATE(
                deserialize_set_font(operation_queue, operands, arena)
            );
            return NULL;
        }
        case PDF_OPERATOR_Td: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_Td);
            PDF_PROPAGATE(
                deserialize_vec2(&queue_op->data.text_offset, operands, arena)
            );
            return NULL;
        }
        case PDF_OPERATOR_Tm: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_Tm);
            PDF_PROPAGATE(deserialize_matrix(
                &queue_op->data.set_text_matrix,
                operands,
                arena
            ));
            return NULL;
        }
        case PDF_OPERATOR_Tj: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_TJ);
            PDF_PROPAGATE(deserialize_text_op(
                &queue_op->data.positioned_text,
                operands,
                arena
            ));
            return NULL;
        }
        case PDF_OPERATOR_TJ: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_TJ);
            PDF_PROPAGATE(deserialize_positioned_text_op(
                &queue_op->data.positioned_text,
                operands,
                arena
            ));
            return NULL;
        }
        case PDF_OPERATOR_g: {
            PDF_PROPAGATE(
                deserialize_set_gray(false, operation_queue, operands, arena)
            );
            return NULL;
        }
        case PDF_OPERATOR_RG: {
            PDF_PROPAGATE(
                deserialize_set_rgb(true, operation_queue, operands, arena)
            );
            return NULL;
        }
        case PDF_OPERATOR_rg: {
            PDF_PROPAGATE(
                deserialize_set_rgb(false, operation_queue, operands, arena)
            );
            return NULL;
        }
        case PDF_OPERATOR_Do: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_Do);
            PDF_PROPAGATE(
                deserialize_name(&queue_op->data.paint_xobject, operands, arena)
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
