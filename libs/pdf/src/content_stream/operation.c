#include "pdf/content_stream/operation.h"

#include <stdbool.h>

#include "../deser.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "logger/log.h"
#include "operation.h"
#include "pdf/content_stream/operator.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

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

static Error* deserde_line_cap_style(
    PdfLineCapStyle* target_ptr,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfInteger type;
    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(&type, PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER))
    };

    TRY(pdf_deserde_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        resolver
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
            return ERROR(
                PDF_ERR_INVALID_NUMBER,
                "Line cap style must be in range 0-2 inclusive"
            );
        }
    }

    return NULL;
}

static Error* deserde_line_join_style(
    PdfLineJoinStyle* target_ptr,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfInteger type;
    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(&type, PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER))
    };

    TRY(pdf_deserde_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        resolver
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
            return ERROR(
                PDF_ERR_INVALID_NUMBER,
                "Line join style must be in range 0-2 inclusive"
            );
        }
    }

    return NULL;
}

static Error* deserde_matrix(
    GeomMat3* target,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(target);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfReal a, b, c, d, e, f;
    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(&a, PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)),
        PDF_OPERAND(&b, PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)),
        PDF_OPERAND(&c, PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)),
        PDF_OPERAND(&d, PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)),
        PDF_OPERAND(&e, PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)),
        PDF_OPERAND(&f, PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline))
    };

    TRY(pdf_deserde_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        resolver
    ));

    *target = geom_mat3_new_pdf(a, b, c, d, e, f);

    return NULL;
}

static Error* deserde_cubic_bezier(
    PdfOpParamsCubicBezier* target_ptr,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            &target_ptr->c1.x,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &target_ptr->c1.y,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &target_ptr->c2.x,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &target_ptr->c2.y,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &target_ptr->end.x,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &target_ptr->end.y,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        )
    };

    TRY(pdf_deserde_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        resolver
    ));

    return NULL;
}

static Error* deserde_part_cubic_bezier(
    PdfOpParamsPartCubicBezier* target_ptr,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            &target_ptr->a.x,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &target_ptr->a.y,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &target_ptr->b.x,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &target_ptr->b.y,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        )
    };

    TRY(pdf_deserde_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        resolver
    ));

    return NULL;
}

static Error* deserde_draw_rectangle(
    PdfContentOpVec* operation_queue,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(operation_queue);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfNumber x, y, width, height;

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(&x, PDF_DESERDE_CUSTOM(pdf_deserde_number_trampoline)),
        PDF_OPERAND(&y, PDF_DESERDE_CUSTOM(pdf_deserde_number_trampoline)),
        PDF_OPERAND(&width, PDF_DESERDE_CUSTOM(pdf_deserde_number_trampoline)),
        PDF_OPERAND(&height, PDF_DESERDE_CUSTOM(pdf_deserde_number_trampoline))
    };

    TRY(pdf_deserde_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        resolver
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

static Error* deserde_set_font(
    PdfContentOpVec* operation_queue,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(operation_queue);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfContentOp* queue_op = new_queue_op(operation_queue, PDF_OPERATOR_Tf);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            &queue_op->data.set_font.font,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_OPERAND(
            &queue_op->data.set_font.size,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        )
    };

    TRY(pdf_deserde_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        resolver
    ));

    return NULL;
}

static Error* deserde_vec2(
    GeomVec2* target_ptr,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            &target_ptr->x,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &target_ptr->y,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        )
    };

    TRY(pdf_deserde_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        resolver
    ));

    return NULL;
}

static Error* deserde_text_op(
    PdfOpParamsPositionedTextVec** target_vec,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(target_vec);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfString string;
    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(&string, PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_STRING))
    };
    TRY(pdf_deserde_operands(operands, descriptors, 1, resolver));

    PdfOpParamsPositionedTextElement* element =
        pdf_op_params_positioned_text_vec_push_uninit(
            (void*)target_vec,
            pdf_resolver_arena(resolver)
        );
    element->type = POSITIONED_TEXT_ELEMENT_STR;
    element->value.str = string;

    return NULL;
}

static Error* deserde_positioned_text_element(
    const PdfObject* object,
    PdfOpParamsPositionedTextElement* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

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
            return ERROR(
                PDF_ERR_INCORRECT_TYPE,
                "Expected a string or number, found type %d",
                (int)object->type
            );
        }
    }

    return NULL;
}

DESERDE_IMPL_TRAMPOLINE(
    deserde_positioned_text_element_trampoline,
    deserde_positioned_text_element
)

static Error* deserde_positioned_text_op(
    PdfOpParamsPositionedTextVec** target_vec,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(target_vec);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfOperandDescriptor descriptors[] = {PDF_OPERAND(
        target_vec,
        PDF_DESERDE_ARRAY(
            pdf_op_params_positioned_text_vec_push_uninit,
            PDF_DESERDE_CUSTOM(deserde_positioned_text_element_trampoline)
        )
    )};

    TRY(pdf_deserde_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        resolver
    ));

    return NULL;
}

static Error* deserde_set_gray(
    bool stroking,
    PdfContentOpVec* operation_queue,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(operation_queue);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfContentOp* queue_op = new_queue_op(
        operation_queue,
        stroking ? PDF_OPERATOR_g : PDF_OPERATOR_G
    );

    PdfOperandDescriptor descriptors[] = {PDF_OPERAND(
        &queue_op->data.set_gray,
        PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
    )};

    TRY(pdf_deserde_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        resolver
    ));

    return NULL;
}

static Error* deserde_set_rgb(
    bool stroking,
    PdfContentOpVec* operation_queue,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(operation_queue);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfContentOp* queue_op = new_queue_op(
        operation_queue,
        stroking ? PDF_OPERATOR_RG : PDF_OPERATOR_rg
    );

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            &queue_op->data.set_rgb.r,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &queue_op->data.set_rgb.g,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &queue_op->data.set_rgb.b,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        )
    };

    TRY(pdf_deserde_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        resolver
    ));

    return NULL;
}

static Error* deserde_set_cmyk(
    bool stroking,
    PdfContentOpVec* operation_queue,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(operation_queue);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfContentOp* queue_op = new_queue_op(
        operation_queue,
        stroking ? PDF_OPERATOR_K : PDF_OPERATOR_k
    );

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(
            &queue_op->data.set_cmyk.c,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &queue_op->data.set_cmyk.m,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &queue_op->data.set_cmyk.y,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        ),
        PDF_OPERAND(
            &queue_op->data.set_cmyk.k,
            PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
        )
    };

    TRY(pdf_deserde_operands(
        operands,
        descriptors,
        sizeof(descriptors) / sizeof(PdfOperandDescriptor),
        resolver
    ));

    return NULL;
}

static Error* deserde_num_as_real(
    PdfReal* target,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(target);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfOperandDescriptor descriptors[] = {PDF_OPERAND(
        target,
        PDF_DESERDE_CUSTOM(pdf_deserde_num_as_real_trampoline)
    )};
    TRY(pdf_deserde_operands(operands, descriptors, 1, resolver));

    return NULL;
}

static Error* deserde_name(
    PdfName* target,
    const PdfObjectVec* operands,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(target);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);

    PdfOperandDescriptor descriptors[] = {
        PDF_OPERAND(target, PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME))
    };
    TRY(pdf_deserde_operands(operands, descriptors, 1, resolver));

    return NULL;
}

Error* pdf_deserde_content_op(
    PdfOperator op,
    const PdfObjectVec* operands,
    PdfResolver* resolver,
    PdfContentOpVec* operation_queue
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(operation_queue);

    // TODO: For zero-operand operators, check that they have zero operands.
    switch (op) {
        case PDF_OPERATOR_w: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_w);
            TRY(deserde_num_as_real(
                &queue_op->data.set_line_width,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_J: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_J);
            TRY(deserde_line_cap_style(
                &queue_op->data.set_line_cap,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_j: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_J);
            TRY(deserde_line_join_style(
                &queue_op->data.set_join_style,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_M: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_M);
            TRY(deserde_num_as_real(
                &queue_op->data.miter_limit,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_d: {
            LOG_WARN(RENDER, "TODO: Render dashed lines");
            return NULL;
        }
        case PDF_OPERATOR_i: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_i);
            TRY(deserde_num_as_real(
                &queue_op->data.flatness,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_gs: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_gs);
            TRY(deserde_name(&queue_op->data.set_gstate, operands, resolver));
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
            TRY(deserde_matrix(&queue_op->data.set_ctm, operands, resolver));
            return NULL;
        }
        case PDF_OPERATOR_m: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_m);
            TRY(deserde_vec2(&queue_op->data.new_subpath, operands, resolver));
            return NULL;
        }
        case PDF_OPERATOR_l: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_l);
            TRY(deserde_vec2(&queue_op->data.line_to, operands, resolver));
            return NULL;
        }
        case PDF_OPERATOR_c: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_c);
            TRY(deserde_cubic_bezier(
                &queue_op->data.cubic_bezier,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_v:
        case PDF_OPERATOR_y: {
            PdfContentOp* queue_op = new_queue_op(operation_queue, op);
            TRY(deserde_part_cubic_bezier(
                &queue_op->data.part_cubic_bezier,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_h: {
            new_queue_op(operation_queue, PDF_OPERATOR_h);
            return NULL;
        }
        case PDF_OPERATOR_re: {
            TRY(deserde_draw_rectangle(operation_queue, operands, resolver));
            return NULL;
        }
        case PDF_OPERATOR_S: {
            new_queue_op(operation_queue, PDF_OPERATOR_S);
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
        case PDF_OPERATOR_n: {
            new_queue_op(operation_queue, PDF_OPERATOR_n);
            return NULL;
        }
        case PDF_OPERATOR_W: {
            LOG_WARN(PDF, "TODO: Clipping paths");
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
        case PDF_OPERATOR_Tc:
        case PDF_OPERATOR_Tw:
        case PDF_OPERATOR_Tz:
        case PDF_OPERATOR_TL: {
            PdfContentOp* queue_op = new_queue_op(operation_queue, op);
            TRY(deserde_num_as_real(
                &queue_op->data.set_text_metric,
                operands,
                resolver
            ));
            return NULL;
        };
        case PDF_OPERATOR_Tf: {
            TRY(deserde_set_font(operation_queue, operands, resolver));
            return NULL;
        }
        case PDF_OPERATOR_Td:
        case PDF_OPERATOR_TD: {
            PdfContentOp* queue_op = new_queue_op(operation_queue, op);
            TRY(deserde_vec2(&queue_op->data.text_offset, operands, resolver));
            return NULL;
        }
        case PDF_OPERATOR_Tm: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_Tm);
            TRY(deserde_matrix(
                &queue_op->data.set_text_matrix,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_T_star: {
            new_queue_op(operation_queue, PDF_OPERATOR_T_star);
            return NULL;
        }
        case PDF_OPERATOR_Tj: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_TJ);
            TRY(deserde_text_op(
                &queue_op->data.positioned_text,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_TJ: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_TJ);
            TRY(deserde_positioned_text_op(
                &queue_op->data.positioned_text,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_CS:
        case PDF_OPERATOR_cs: {
            PdfContentOp* queue_op = new_queue_op(operation_queue, op);
            TRY(deserde_name(
                &queue_op->data.set_color_space,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_SC:
        case PDF_OPERATOR_SCN:
        case PDF_OPERATOR_sc:
        case PDF_OPERATOR_scn: {
            PdfContentOp* queue_op = new_queue_op(operation_queue, op);
            queue_op->data.set_color = pdf_object_vec_clone(operands);
            return NULL;
        }
        case PDF_OPERATOR_G:
        case PDF_OPERATOR_g: {
            TRY(deserde_set_gray(
                op == PDF_OPERATOR_G,
                operation_queue,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_RG:
        case PDF_OPERATOR_rg: {
            TRY(deserde_set_rgb(
                op == PDF_OPERATOR_RG,
                operation_queue,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_K:
        case PDF_OPERATOR_k: {
            TRY(deserde_set_cmyk(
                op == PDF_OPERATOR_K,
                operation_queue,
                operands,
                resolver
            ));
            return NULL;
        }
        case PDF_OPERATOR_sh: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_sh);
            TRY(
                deserde_name(&queue_op->data.paint_shading, operands, resolver)
            );
            return NULL;
        }
        case PDF_OPERATOR_Do: {
            PdfContentOp* queue_op =
                new_queue_op(operation_queue, PDF_OPERATOR_Do);
            TRY(
                deserde_name(&queue_op->data.paint_xobject, operands, resolver)
            );
            return NULL;
        }
        case PDF_OPERATOR_BDC:
        case PDF_OPERATOR_EMC: {
            LOG_WARN(PDF, "TODO: Marked content");
            return NULL;
        }
        case PDF_OPERATOR_BX:
        case PDF_OPERATOR_EX: {
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
