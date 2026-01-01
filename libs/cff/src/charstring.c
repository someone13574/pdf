#include "charstring.h"

#include <stdbool.h>
#include <stdint.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "canvas/path_builder.h"
#include "err/error.h"
#include "geom/vec2.h"
#include "index.h"
#include "logger/log.h"
#include "parser.h"
#include "types.h"

enum { CHARSTR_MAX_OPERANDS = 48 };

#define CHARSTR_OPERATORS                                                      \
    X(RESERVED0)                                                               \
    X(HSTEM)                                                                   \
    X(RESERVED2)                                                               \
    X(VSTEM)                                                                   \
    X(VMOVETO)                                                                 \
    X(RLINETO)                                                                 \
    X(HLINETO)                                                                 \
    X(VLINETO)                                                                 \
    X(RRCURVETO)                                                               \
    X(RESERVED9)                                                               \
    X(CALLSUBR)                                                                \
    X(RETURN)                                                                  \
    X(ESCAPE)                                                                  \
    X(RESERVED13)                                                              \
    X(ENDCHAR)                                                                 \
    X(RESERVED15)                                                              \
    X(RESERVED16)                                                              \
    X(RESERVED17)                                                              \
    X(HSTEMHM)                                                                 \
    X(HINTMASK)                                                                \
    X(CNTRMASK)                                                                \
    X(RMOVETO)                                                                 \
    X(HMOVETO)                                                                 \
    X(VSTEMHM)                                                                 \
    X(RCURVELINE)                                                              \
    X(RLINECURVE)                                                              \
    X(VVCURVETO)                                                               \
    X(HHCURVETO)                                                               \
    X(SHORTINT)                                                                \
    X(CALLGSUBR)                                                               \
    X(VHCURVETO)                                                               \
    X(HVCURVETO)                                                               \
    X(RESERVED_ESC0)                                                           \
    X(RESERVED_ESC1)                                                           \
    X(RESERVED_ESC2)                                                           \
    X(AND)                                                                     \
    X(OR)                                                                      \
    X(NOT)                                                                     \
    X(RESERVED_ESC6)                                                           \
    X(RESERVED_ESC7)                                                           \
    X(RESERVED_ESC8)                                                           \
    X(ABS)                                                                     \
    X(ADD)                                                                     \
    X(SUB)                                                                     \
    X(DIV)                                                                     \
    X(RESERVED_ESC13)                                                          \
    X(NEG)                                                                     \
    X(EQ)                                                                      \
    X(RESERVED_ESC16)                                                          \
    X(RESERVED_ESC17)                                                          \
    X(DROP)                                                                    \
    X(RESERVED_ESC19)                                                          \
    X(PUT)                                                                     \
    X(GET)                                                                     \
    X(IFELSE)                                                                  \
    X(RANDOM)                                                                  \
    X(MUL)                                                                     \
    X(RESERVED_ESC23)                                                          \
    X(SQRT)                                                                    \
    X(DUP)                                                                     \
    X(EXCH)                                                                    \
    X(INDEX)                                                                   \
    X(ROLL)                                                                    \
    X(RESERVED_ESC31)                                                          \
    X(RESERVED_ESC32)                                                          \
    X(RESERVED_ESC33)                                                          \
    X(HFLEX)                                                                   \
    X(FLEX)                                                                    \
    X(HFLEX1)                                                                  \
    X(FLEX1)

#define X(name) CHARSTR_OPERATOR_##name,
typedef enum { CHARSTR_OPERATORS } CharstrOperator;
#undef X

#define X(name) #name,
static const char* charstr_operator_names[] = {CHARSTR_OPERATORS};
#undef X
#undef CHARSTR_OPERATORS

typedef struct {
    enum { CHARSTR_OPERAND_INT } type;
    union {
        int32_t integer; // TODO: Use Card16?
    } value;
} CharstrOperand;

typedef struct {
    CharstrOperand operand_stack[CHARSTR_MAX_OPERANDS];
    size_t operand_count;
    size_t stack_bottom;

    bool width_set;
    double width;

    PathBuilder* path_builder;
    GeomVec2 current_point;
} CharstrState;

static Error* cff_charstr2_subr(
    CffParser* parser,
    CffIndex global_subr_index,
    CffIndex local_subr_index,
    size_t length,
    CharstrState* state,
    bool* endchar_out
);

static double operand_as_real(CharstrOperand operand) {
    switch (operand.type) {
        case CHARSTR_OPERAND_INT: {
            return (double)operand.value.integer;
        }
    }

    LOG_PANIC("Unreachable");
}

static Error* push_integer_operand(int32_t value, CharstrState* state) {
    RELEASE_ASSERT(state);

    if (state->operand_count == CHARSTR_MAX_OPERANDS) {
        return ERROR(
            PDF_ERR_EXCESS_OPERAND,
            "An operator may be preceded by up to a maximum of 48 operands"
        );
    }

    state->operand_stack[state->operand_count++] =
        (CharstrOperand) {.type = CHARSTR_OPERAND_INT, .value.integer = value};

    LOG_DIAG(
        TRACE,
        CFF,
        "Pushed integer: %d",
        (int)state->operand_stack[state->operand_count - 1].value.integer
    );

    return NULL;
}

static size_t num_operands_available(CharstrState* state) {
    RELEASE_ASSERT(state);
    return state->operand_count - state->stack_bottom;
}

static Error*
check_operands_available(size_t required_count, CharstrState* state) {
    RELEASE_ASSERT(state);

    if (num_operands_available(state) < required_count) {
        return ERROR(CFF_ERR_MISSING_OPERAND);
    }

    return NULL;
}

static Error* handle_width(size_t required_operands, CharstrState* state) {
    RELEASE_ASSERT(state);

    if (!state->width_set
        && num_operands_available(state) > required_operands) {
        LOG_DIAG(TRACE, CFF, "Reading width in charstring");

        TRY(check_operands_available(1, state));

        state->width =
            operand_as_real(state->operand_stack[state->stack_bottom++]);
        state->width_set = true;
    }

    return NULL;
}

static Error* get_relative_position(CharstrState* state, GeomVec2* point_out) {
    RELEASE_ASSERT(state);

    TRY(check_operands_available(2, state));

    state->current_point = geom_vec2_add(
        state->current_point,
        geom_vec2_new(
            operand_as_real(state->operand_stack[state->stack_bottom]),
            operand_as_real(state->operand_stack[state->stack_bottom + 1])
        )
    );
    state->stack_bottom += 2;

    if (point_out) {
        *point_out = state->current_point;
    }
    LOG_DIAG(
        TRACE,
        CFF,
        "New position: (%f, %f) (point)",
        state->current_point.x,
        state->current_point.y
    );

    return NULL;
}

static Error*
get_relative_position_yx(CharstrState* state, GeomVec2* point_out) {
    RELEASE_ASSERT(state);

    TRY(check_operands_available(2, state));

    state->current_point = geom_vec2_add(
        state->current_point,
        geom_vec2_new(
            operand_as_real(state->operand_stack[state->stack_bottom + 1]),
            operand_as_real(state->operand_stack[state->stack_bottom])
        )
    );
    state->stack_bottom += 2;

    if (point_out) {
        *point_out = state->current_point;
    }
    LOG_DIAG(
        TRACE,
        CFF,
        "New position: (%f, %f) (yx point)",
        state->current_point.x,
        state->current_point.y
    );

    return NULL;
}

static Error* get_relative_x(CharstrState* state, GeomVec2* point_out) {
    RELEASE_ASSERT(state);

    TRY(check_operands_available(1, state));

    state->current_point = geom_vec2_add(
        state->current_point,
        geom_vec2_new(
            operand_as_real(state->operand_stack[state->stack_bottom++]),
            0.0
        )
    );

    if (point_out) {
        *point_out = state->current_point;
    }
    LOG_DIAG(
        TRACE,
        CFF,
        "New position: (%f, %f) (x)",
        state->current_point.x,
        state->current_point.y
    );

    return NULL;
}

static Error* get_relative_y(CharstrState* state, GeomVec2* point_out) {
    RELEASE_ASSERT(state);

    TRY(check_operands_available(1, state));

    state->current_point = geom_vec2_add(
        state->current_point,
        geom_vec2_new(
            0.0,
            operand_as_real(state->operand_stack[state->stack_bottom++])
        )
    );

    if (point_out) {
        *point_out = state->current_point;
    }
    LOG_DIAG(
        TRACE,
        CFF,
        "New position: (%f, %f) (y)",
        state->current_point.x,
        state->current_point.y
    );

    return NULL;
}

static Error* pop_operand(CharstrState* state, CharstrOperand* operand_out) {
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(operand_out);

    TRY(check_operands_available(1, state));
    *operand_out = state->operand_stack[--state->operand_count];

    return NULL;
}

static Error* check_stack_consumed(CharstrState* state) {
    RELEASE_ASSERT(state);

    if (num_operands_available(state) != 0) {
        return ERROR(PDF_ERR_EXCESS_OPERAND, "Operand must consume stack");
    }

    state->operand_count = 0;
    state->stack_bottom = 0;

    LOG_DIAG(TRACE, CFF, "Stack empty");

    return NULL;
}

static CffCard16 subr_bias(CffCard16 num_subrs) {
    if (num_subrs < 1240) {
        return 107;
    } else if (num_subrs < 33900) {
        return 1131;
    } else {
        return 32768;
    }
}

static Error* charstring_interpret_operator(
    CharstrOperator operator,
    CharstrState* state,
    CffParser* parser,
    CffIndex global_subr_index,
    CffIndex local_subr_index,
    bool* endchar_out,
    bool* return_out
) {
    RELEASE_ASSERT(operator < 70);
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(state->stack_bottom == 0);
    RELEASE_ASSERT(endchar_out);
    RELEASE_ASSERT(return_out);

    *endchar_out = false;
    *return_out = false;

    LOG_DIAG(
        DEBUG,
        CFF,
        "Operator: %s (stack=%zu)",
        charstr_operator_names[operator],
        num_operands_available(state)
    );

    switch (operator) {
        case CHARSTR_OPERATOR_VMOVETO: {
            TRY(handle_width(1, state));
            TRY(get_relative_y(state, NULL));
            path_builder_new_contour(state->path_builder, state->current_point);
            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_RLINETO: {
            do {
                TRY(get_relative_position(state, NULL));
                path_builder_line_to(state->path_builder, state->current_point);
            } while (num_operands_available(state) >= 2);

            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_HLINETO: {
            bool horizontal = true;
            do {
                if (horizontal) {
                    TRY(get_relative_x(state, NULL));
                } else {
                    TRY(get_relative_y(state, NULL));
                }
                path_builder_line_to(state->path_builder, state->current_point);
                horizontal = !horizontal;
            } while (num_operands_available(state) >= 1);

            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_VLINETO: {
            bool horizontal = false;
            do {
                if (horizontal) {
                    TRY(get_relative_x(state, NULL));
                } else {
                    TRY(get_relative_y(state, NULL));
                }
                path_builder_line_to(state->path_builder, state->current_point);
                horizontal = !horizontal;
            } while (num_operands_available(state) >= 1);

            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_RRCURVETO: {
            do {
                GeomVec2 control_a;
                GeomVec2 control_b;
                TRY(get_relative_position(state, &control_a));
                TRY(get_relative_position(state, &control_b));
                TRY(get_relative_position(state, NULL));

                path_builder_cubic_bezier_to(
                    state->path_builder,
                    state->current_point,
                    control_a,
                    control_b
                );
            } while (num_operands_available(state) >= 6);

            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_CALLSUBR: {
            CharstrOperand unbiased_subr_num;
            TRY(pop_operand(state, &unbiased_subr_num));
            if (unbiased_subr_num.type != CHARSTR_OPERAND_INT) {
                return ERROR(
                    CFF_ERR_INCORRECT_OPERAND,
                    "Expected an integer operand"
                );
            }

            CffCard16 bias = subr_bias(local_subr_index.count);
            int64_t subr_idx = unbiased_subr_num.value.integer + bias;
            if (subr_idx < 0 || subr_idx >= local_subr_index.count) {
                return ERROR(
                    CFF_ERR_INVALID_SUBR,
                    "Invalid local subroutine #%lld",
                    (long long int)subr_idx
                );
            }

            size_t return_offset = parser->offset;
            size_t subr_size;
            TRY(cff_index_seek_object(
                &local_subr_index,
                parser,
                (CffCard16)subr_idx,
                &subr_size
            ));
            TRY(cff_charstr2_subr(
                parser,
                global_subr_index,
                local_subr_index,
                subr_size,
                state,
                endchar_out
            ));
            TRY(cff_parser_seek(parser, return_offset));
            break;
        }
        case CHARSTR_OPERATOR_RETURN: {
            *return_out = true;
            break;
        }
        case CHARSTR_OPERATOR_ENDCHAR: {
            *endchar_out = true;
            TRY(handle_width(0, state));
            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_RMOVETO: {
            TRY(handle_width(2, state));
            TRY(get_relative_position(state, NULL));
            path_builder_new_contour(state->path_builder, state->current_point);
            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_HMOVETO: {
            TRY(handle_width(1, state));
            TRY(get_relative_x(state, NULL));
            path_builder_new_contour(state->path_builder, state->current_point);
            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_RCURVELINE: {
            do {
                GeomVec2 control_a;
                GeomVec2 control_b;
                TRY(get_relative_position(state, &control_a));
                TRY(get_relative_position(state, &control_b));
                TRY(get_relative_position(state, NULL));

                path_builder_cubic_bezier_to(
                    state->path_builder,
                    state->current_point,
                    control_a,
                    control_b
                );
            } while (num_operands_available(state) >= 6);

            TRY(get_relative_position(state, NULL));
            path_builder_line_to(state->path_builder, state->current_point);

            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_RLINECURVE: {
            do {
                TRY(get_relative_position(state, NULL));
                path_builder_line_to(state->path_builder, state->current_point);
            } while (num_operands_available(state) >= 8);

            GeomVec2 control_a;
            GeomVec2 control_b;
            TRY(get_relative_position(state, &control_a));
            TRY(get_relative_position(state, &control_b));
            TRY(get_relative_position(state, NULL));

            path_builder_cubic_bezier_to(
                state->path_builder,
                state->current_point,
                control_a,
                control_b
            );

            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_VVCURVETO: {
            bool read_x_delta = num_operands_available(state) % 4 == 1;
            do {
                GeomVec2 control_a;
                if (read_x_delta) {
                    // If there is an extra operand at the front, the first
                    // point is a full point and not just a y-delta.
                    TRY(get_relative_position(state, &control_a));
                    read_x_delta = false;
                } else {
                    TRY(get_relative_y(state, &control_a));
                }

                GeomVec2 control_b;
                TRY(get_relative_position(state, &control_b));
                TRY(get_relative_y(state, NULL));

                path_builder_cubic_bezier_to(
                    state->path_builder,
                    state->current_point,
                    control_a,
                    control_b
                );
            } while (num_operands_available(state) >= 4);

            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_HHCURVETO: {
            bool read_y_delta = num_operands_available(state) % 4 == 1;
            do {
                GeomVec2 control_a;
                if (read_y_delta) {
                    // If there is an extra operand at the front, the first
                    // point is a full point and not just a x-delta. We also
                    // read the operands backwards since that is how they are
                    // stored in this case.
                    TRY(get_relative_position_yx(state, &control_a));
                    read_y_delta = false;
                } else {
                    TRY(get_relative_x(state, &control_a));
                }

                GeomVec2 control_b;
                TRY(get_relative_position(state, &control_b));
                TRY(get_relative_x(state, NULL));

                path_builder_cubic_bezier_to(
                    state->path_builder,
                    state->current_point,
                    control_a,
                    control_b
                );
            } while (num_operands_available(state) >= 4);

            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_CALLGSUBR: {
            CharstrOperand unbiased_subr_num;
            TRY(pop_operand(state, &unbiased_subr_num));
            if (unbiased_subr_num.type != CHARSTR_OPERAND_INT) {
                return ERROR(
                    CFF_ERR_INCORRECT_OPERAND,
                    "Expected an integer operand"
                );
            }

            CffCard16 bias = subr_bias(global_subr_index.count);
            int64_t subr_idx = unbiased_subr_num.value.integer + bias;
            if (subr_idx < 0 || subr_idx >= global_subr_index.count) {
                return ERROR(
                    CFF_ERR_INVALID_SUBR,
                    "Invalid global subroutine #%lld",
                    (long long int)subr_idx
                );
            }

            size_t return_offset = parser->offset;
            size_t subr_size;
            TRY(cff_index_seek_object(
                &global_subr_index,
                parser,
                (CffCard16)subr_idx,
                &subr_size
            ));
            TRY(cff_charstr2_subr(
                parser,
                global_subr_index,
                local_subr_index,
                subr_size,
                state,
                endchar_out
            ));
            TRY(cff_parser_seek(parser, return_offset));

            break;
        }
        case CHARSTR_OPERATOR_VHCURVETO: {
            bool horizontal = false;
            do {
                GeomVec2 control_a;
                if (horizontal) {
                    TRY(get_relative_x(state, &control_a));
                } else {
                    TRY(get_relative_y(state, &control_a));
                }

                GeomVec2 control_b;
                TRY(get_relative_position(state, &control_b));

                if (num_operands_available(state) == 2) {
                    // Handle full point at end
                    if (horizontal) {
                        TRY(get_relative_position_yx(state, NULL));
                    } else {
                        TRY(get_relative_position(state, NULL));
                    }
                } else if (horizontal) {
                    // Horizontal delta is now zero
                    TRY(get_relative_y(state, NULL));
                } else {
                    TRY(get_relative_x(state, NULL));
                }

                path_builder_cubic_bezier_to(
                    state->path_builder,
                    state->current_point,
                    control_a,
                    control_b
                );
                horizontal = !horizontal;
            } while (num_operands_available(state) >= 4);

            TRY(check_stack_consumed(state));
            break;
        }
        case CHARSTR_OPERATOR_HVCURVETO: {
            bool horizontal = true;
            do {
                GeomVec2 control_a;
                if (horizontal) {
                    TRY(get_relative_x(state, &control_a));
                } else {
                    TRY(get_relative_y(state, &control_a));
                }

                GeomVec2 control_b;
                TRY(get_relative_position(state, &control_b));

                if (num_operands_available(state) == 2) {
                    // Handle full point at end
                    if (horizontal) {
                        TRY(get_relative_position_yx(state, NULL));
                    } else {
                        TRY(get_relative_position(state, NULL));
                    }
                } else if (horizontal) {
                    // Horizontal delta is now zero
                    TRY(get_relative_y(state, NULL));
                } else {
                    TRY(get_relative_x(state, NULL));
                }

                path_builder_cubic_bezier_to(
                    state->path_builder,
                    state->current_point,
                    control_a,
                    control_b
                );
                horizontal = !horizontal;
            } while (num_operands_available(state) >= 4);

            TRY(check_stack_consumed(state));
            break;
        }
        default: {
            LOG_TODO(
                "Operator: %s (%d)",
                charstr_operator_names[operator],
                (int)operator
            );
        }
    }

    return NULL;
}

static Error* cff_charstr2_subr(
    CffParser* parser,
    CffIndex global_subr_index,
    CffIndex local_subr_index,
    size_t length,
    CharstrState* state,
    bool* endchar_out
) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(endchar_out);

    size_t end_offset = parser->offset + length;
    while (parser->offset < end_offset) {
        CffCard8 byte;
        TRY(cff_parser_read_card8(parser, &byte));

        bool endchar = false;
        bool return_subr = false;

        if (byte <= 11) {
            TRY(charstring_interpret_operator(
                byte,
                state,
                parser,
                global_subr_index,
                local_subr_index,
                &endchar,
                &return_subr
            ));
        } else if (byte == 12) {
            CffCard8 next_byte;
            TRY(cff_parser_read_card8(parser, &next_byte));

            LOG_DIAG(
                INFO,
                CFF,
                "Operator: %s",
                charstr_operator_names[next_byte + 32]
            );
            LOG_TODO();
        } else if (byte <= 18) {
            TRY(charstring_interpret_operator(
                byte,
                state,
                parser,
                global_subr_index,
                local_subr_index,
                &endchar,
                &return_subr
            ));
        } else if (byte <= 20) {
            LOG_TODO();
        } else if (byte <= 27) {
            TRY(charstring_interpret_operator(
                byte,
                state,
                parser,
                global_subr_index,
                local_subr_index,
                &endchar,
                &return_subr
            ));
        } else if (byte == 28) {
            LOG_TODO();
        } else if (byte <= 31) {
            TRY(charstring_interpret_operator(
                byte,
                state,
                parser,
                global_subr_index,
                local_subr_index,
                &endchar,
                &return_subr
            ));
        } else if (byte <= 246) {
            TRY(push_integer_operand((int32_t)byte - 139, state));
        } else if (byte <= 250) {
            CffCard8 next_byte;
            TRY(cff_parser_read_card8(parser, &next_byte));
            TRY(push_integer_operand(
                ((int32_t)byte - 247) * 256 + (int32_t)next_byte + 108,
                state
            ));
        } else if (byte <= 254) {
            CffCard8 next_byte;
            TRY(cff_parser_read_card8(parser, &next_byte));
            TRY(push_integer_operand(
                -(((int32_t)byte - 251) * 256) - (int32_t)next_byte - 108,
                state
            ));
        } else {
            LOG_TODO("4 byte twos complement");
        }

        if (endchar) {
            *endchar_out = true;
            break;
        } else if (return_subr) {
            break;
        }
    }

    return NULL;
}

Error* cff_charstr2_render(
    CffParser* parser,
    CffIndex global_subr_index,
    CffIndex local_subr_index,
    size_t length,
    Canvas* canvas,
    GeomMat3 transform,
    CanvasBrush brush
) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(canvas);

    Arena* temp_arena = arena_new(4096);
    CharstrState state =
        (CharstrState) {.operand_count = 0,
                        .stack_bottom = 0,
                        .width_set = false,
                        .width = 0.0,
                        .path_builder = path_builder_new(temp_arena),
                        .current_point = geom_vec2_new(0.0, 0.0)};

    bool endchar; // We don't actually need this since we aren't actually
                  // calling a subroutine.
    TRY(cff_charstr2_subr(
        parser,
        global_subr_index,
        local_subr_index,
        length,
        &state,
        &endchar
    ));

    path_builder_apply_transform(state.path_builder, transform);
    canvas_draw_path(canvas, state.path_builder, brush);
    arena_free(temp_arena);

    return NULL;
}
