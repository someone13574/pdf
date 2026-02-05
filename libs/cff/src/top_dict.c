#include "top_dict.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "err/error.h"
#include "geom/mat3.h"
#include "geom/rect.h"
#include "geom/vec2.h"
#include "logger/log.h"
#include "parse_ctx/binary.h"
#include "types.h"

enum { CFF_MAX_OPERANDS = 48 };

#define CFF_TOP_DICT_KEYS                                                      \
    X(VERSION)                                                                 \
    X(NOTICE)                                                                  \
    X(COPYRIGHT)                                                               \
    X(FULL_NAME)                                                               \
    X(FAMILY_NAME)                                                             \
    X(WEIGHT)                                                                  \
    X(IS_FIXED_PITCH)                                                          \
    X(ITALIC_ANGLE)                                                            \
    X(UNDERLINE_POSITION)                                                      \
    X(UNDERLINE_THICKNESS)                                                     \
    X(PAINT_TYPE)                                                              \
    X(CHARSTRING_TYPE)                                                         \
    X(FONT_MATRIX)                                                             \
    X(UNIQUE_ID)                                                               \
    X(FONT_BBOX)                                                               \
    X(STROKE_WIDTH)                                                            \
    X(XUID)                                                                    \
    X(CHARSET)                                                                 \
    X(ENCODING)                                                                \
    X(CHAR_STRINGS)                                                            \
    X(PRIVATE)                                                                 \
    X(SYNTHETIC_BASE)                                                          \
    X(POSTSCRIPT)                                                              \
    X(BASE_FONT_NAME)                                                          \
    X(BASE_FONT_BLEND)

#define X(name) CFF_TOP_DICT_##name,
typedef enum { CFF_TOP_DICT_KEYS } CffTopDictKey;
#undef X

#define X(name) #name,
static const char* cff_top_dict_key_names[] = {CFF_TOP_DICT_KEYS};
#undef X
#undef CFF_TOP_DICT_KEYS

static Error* cff_top_dict_interpret_key(
    ParseCtx* ctx,
    uint8_t operator0,
    CffTopDictKey* key_out
) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(operator0 <= 21);
    RELEASE_ASSERT(key_out);

    switch (operator0) {
        case 0: {
            *key_out = CFF_TOP_DICT_VERSION;
            return NULL;
        }
        case 1: {
            *key_out = CFF_TOP_DICT_NOTICE;
            return NULL;
        }
        case 2: {
            *key_out = CFF_TOP_DICT_FULL_NAME;
            return NULL;
        }
        case 3: {
            *key_out = CFF_TOP_DICT_FAMILY_NAME;
            return NULL;
        }
        case 4: {
            *key_out = CFF_TOP_DICT_WEIGHT;
            return NULL;
        }
        case 13: {
            *key_out = CFF_TOP_DICT_UNIQUE_ID;
            return NULL;
        }
        case 5: {
            *key_out = CFF_TOP_DICT_FONT_BBOX;
            return NULL;
        }
        case 14: {
            *key_out = CFF_TOP_DICT_XUID;
            return NULL;
        }
        case 15: {
            *key_out = CFF_TOP_DICT_CHARSET;
            return NULL;
        }
        case 16: {
            *key_out = CFF_TOP_DICT_ENCODING;
            return NULL;
        }
        case 17: {
            *key_out = CFF_TOP_DICT_CHAR_STRINGS;
            return NULL;
        }
        case 18: {
            *key_out = CFF_TOP_DICT_PRIVATE;
            return NULL;
        }
        default: {
            break;
        }
    }

    uint8_t operator1; // Use a Card8 instead of a token since BaseFontName and
                       // BaseFontBlend allow bytes 22 and 23, when the limit
                       // for operators is 21.
    TRY(parse_ctx_read_u8(ctx, &operator1));

    RELEASE_ASSERT(operator0 == 12);
    CffTopDictKey two_byte_keys[] = {
        CFF_TOP_DICT_COPYRIGHT,
        CFF_TOP_DICT_IS_FIXED_PITCH,
        CFF_TOP_DICT_ITALIC_ANGLE,
        CFF_TOP_DICT_UNDERLINE_POSITION,
        CFF_TOP_DICT_UNDERLINE_THICKNESS,
        CFF_TOP_DICT_PAINT_TYPE,
        CFF_TOP_DICT_CHARSTRING_TYPE,
        CFF_TOP_DICT_FONT_BBOX,
        CFF_TOP_DICT_STROKE_WIDTH,
        CFF_TOP_DICT_SYNTHETIC_BASE,
        CFF_TOP_DICT_POSTSCRIPT,
        CFF_TOP_DICT_BASE_FONT_NAME,
        CFF_TOP_DICT_BASE_FONT_BLEND
    };

    if (operator1 <= 8) {
        *key_out = two_byte_keys[operator1];
    } else if (operator1 >= 20 && operator1 <= 23) {
        *key_out =
            two_byte_keys[operator1 - 20 + 9]; // SyntheticBase is 12 20.
                                               // We pack in a single array.
    } else {
        return ERROR(CFF_ERR_EXPECTED_OPERATOR);
    }

    return NULL;
}

static Error*
pop_sid(CffToken* operand_stack, size_t* operand_count, CffSID* sid_out) {
    RELEASE_ASSERT(operand_stack);
    RELEASE_ASSERT(operand_count);
    RELEASE_ASSERT(sid_out);

    if (*operand_count == 0) {
        return ERROR(CFF_ERR_MISSING_OPERAND);
    }

    CffToken token = operand_stack[--*operand_count];
    if (token.type != CFF_TOKEN_INT_OPERAND) {
        return ERROR(CFF_ERR_INCORRECT_OPERAND, "Expected SID operand");
    }

    if (token.value.integer < 0 || token.value.integer >= 65000) {
        return ERROR(CFF_ERR_INVALID_SID, "SIDs must be in the range 0-64999");
    }

    *sid_out = (CffSID)token.value.integer;

    return NULL;
}

// TODO: Share these functions with private dict
static Error*
pop_int(CffToken* operand_stack, size_t* operand_count, int32_t* int_out) {
    RELEASE_ASSERT(operand_stack);
    RELEASE_ASSERT(operand_count);
    RELEASE_ASSERT(int_out);

    if (*operand_count == 0) {
        return ERROR(CFF_ERR_MISSING_OPERAND);
    }

    CffToken token = operand_stack[--*operand_count];
    if (token.type != CFF_TOKEN_INT_OPERAND) {
        return ERROR(CFF_ERR_INCORRECT_OPERAND, "Expected integer operand");
    }

    *int_out = token.value.integer;

    return NULL;
}

static Error* pop_number(
    CffToken* operand_stack,
    size_t* operand_count,
    CffNumber* number_out
) {
    RELEASE_ASSERT(operand_stack);
    RELEASE_ASSERT(operand_count);
    RELEASE_ASSERT(number_out);

    if (*operand_count == 0) {
        return ERROR(CFF_ERR_MISSING_OPERAND);
    }

    CffToken token = operand_stack[--*operand_count];

    switch (token.type) {
        case CFF_TOKEN_INT_OPERAND: {
            number_out->type = CFF_NUMBER_INT;
            number_out->value.integer = token.value.integer;
            break;
        }
        case CFF_TOKEN_REAL_OPERAND: {
            number_out->type = CFF_NUMBER_REAL;
            number_out->value.real = token.value.real;
            break;
        }
        default: {
            return ERROR(CFF_ERR_INCORRECT_OPERAND, "Expected number operand");
        }
    }

    if (token.type != CFF_TOKEN_INT_OPERAND
        && token.type != CFF_TOKEN_REAL_OPERAND) {
        return ERROR(CFF_ERR_INCORRECT_OPERAND, "Expected number operand");
    }

    return NULL;
}

Error*
cff_parse_top_dict(ParseCtx* ctx, size_t length, CffTopDict* top_dict_out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(top_dict_out);

    size_t operand_count = 0;
    CffToken operand_stack[CFF_MAX_OPERANDS] = {0};

    size_t end_offset = ctx->offset + length;
    while (ctx->offset < end_offset) {
        CffToken token;
        TRY(cff_read_token(ctx, &token));

        switch (token.type) {
            case CFF_TOKEN_OPERATOR: {
                CffTopDictKey key;
                TRY(
                    cff_top_dict_interpret_key(ctx, token.value.operator, &key)
                );

                LOG_DIAG(DEBUG, CFF, "Key: %s", cff_top_dict_key_names[key]);
                switch (key) {
                    case CFF_TOP_DICT_VERSION: {
                        TRY(pop_sid(
                            operand_stack,
                            &operand_count,
                            &top_dict_out->version
                        ));
                        break;
                    }
                    case CFF_TOP_DICT_NOTICE: {
                        TRY(pop_sid(
                            operand_stack,
                            &operand_count,
                            &top_dict_out->notice
                        ));
                        break;
                    }
                    case CFF_TOP_DICT_COPYRIGHT: {
                        TRY(pop_sid(
                            operand_stack,
                            &operand_count,
                            &top_dict_out->copyright
                        ));
                        break;
                    }
                    case CFF_TOP_DICT_FULL_NAME: {
                        TRY(pop_sid(
                            operand_stack,
                            &operand_count,
                            &top_dict_out->full_name
                        ));
                        break;
                    }
                    case CFF_TOP_DICT_FAMILY_NAME: {
                        TRY(pop_sid(
                            operand_stack,
                            &operand_count,
                            &top_dict_out->family_name
                        ));
                        break;
                    }
                    case CFF_TOP_DICT_WEIGHT: {
                        TRY(pop_sid(
                            operand_stack,
                            &operand_count,
                            &top_dict_out->weight
                        ));
                        break;
                    }
                    case CFF_TOP_DICT_UNDERLINE_POSITION: {
                        TRY(pop_number(
                            operand_stack,
                            &operand_count,
                            &top_dict_out->underline_position
                        ));
                        break;
                    }
                    case CFF_TOP_DICT_FONT_BBOX: {
                        CffNumber vals[4];
                        for (size_t idx = 0; idx < 4; idx++) {
                            TRY(pop_number(
                                operand_stack,
                                &operand_count,
                                &vals[idx]
                            ));
                        }

                        top_dict_out->font_bbox = geom_rect_new(
                            geom_vec2_new(
                                cff_num_as_real(vals[0]),
                                cff_num_as_real(vals[1])
                            ),
                            geom_vec2_new(
                                cff_num_as_real(vals[1]),
                                cff_num_as_real(vals[2])
                            )
                        );

                        break;
                    }
                    case CFF_TOP_DICT_CHARSET: {
                        TRY(pop_int(
                            operand_stack,
                            &operand_count,
                            &top_dict_out->charset
                        ));
                        break;
                    }
                    case CFF_TOP_DICT_CHAR_STRINGS: {
                        TRY(pop_int(
                            operand_stack,
                            &operand_count,
                            &top_dict_out->char_strings
                        ));
                        break;
                    }
                    case CFF_TOP_DICT_PRIVATE: {
                        TRY(pop_int(
                            operand_stack,
                            &operand_count,
                            &top_dict_out->private_offset
                        ));
                        TRY(pop_int(
                            operand_stack,
                            &operand_count,
                            &top_dict_out->private_dict_size
                        ));
                        break;
                    }
                    default: {
                        LOG_TODO("Key `%s`", cff_top_dict_key_names[key]);
                    }
                }

                break;
            }
            case CFF_TOKEN_INT_OPERAND: {
                LOG_DIAG(
                    TRACE,
                    CFF,
                    "Integer operand: %ld",
                    (long int)token.value.integer
                );

                if (operand_count == CFF_MAX_OPERANDS) {
                    return ERROR(
                        PDF_ERR_EXCESS_OPERAND,
                        "An operator may be preceded by up to a maximum of 48 operands"
                    );
                }
                operand_stack[operand_count++] = token;
                break;
            }
            default: {
                LOG_PANIC("Unknown CFF token type %d", (int)token.type);
            }
        }
    }

    return NULL;
}

CffTopDict cff_top_dict_default(void) {
    return (CffTopDict) {
        .is_fixed_pitch = false,
        .italic_angle = {.type = CFF_NUMBER_INT,    .value.integer = 0},
        .underline_position = {.type = CFF_NUMBER_INT, .value.integer = -100},
        .underline_thickness = {.type = CFF_NUMBER_INT,   .value.integer = 50},
        .paint_type = 0,
        .charstring_type = 2,
        .font_matrix = geom_mat3_new_pdf(0.001, 0.0, 0.0, 0.001, 0.0, 0.0),
        .font_bbox =
            geom_rect_new(geom_vec2_new(0.0, 0.0), geom_vec2_new(0.0, 0.0)),
        .stroke_width = {.type = CFF_NUMBER_INT,    .value.integer = 0},
        .charset = 0,
        .encoding = 0,
        .char_strings = 0,
        .private_dict_size = 0,
        .private_offset = 0
    };
}
