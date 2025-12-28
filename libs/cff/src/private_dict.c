#include "private_dict.h"

#include <stdbool.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "parser.h"
#include "pdf_error/error.h"
#include "types.h"

enum { CFF_MAX_OPERANDS = 48 };

#define CFF_PRIVATE_DICT_KEYS                                                  \
    X(BLUE_VALUES)                                                             \
    X(OTHER_BLUES)                                                             \
    X(FAMILY_BLUES)                                                            \
    X(FAMILY_OTHER_BLUES)                                                      \
    X(BLUE_SCALE)                                                              \
    X(BLUE_SHIFT)                                                              \
    X(BLUE_FUZZ)                                                               \
    X(STD_HW)                                                                  \
    X(STD_VW)                                                                  \
    X(STEM_SNAP_H)                                                             \
    X(STEM_SNAP_V)                                                             \
    X(FORCE_BOLD)                                                              \
    X(LANGUAGE_GROUP)                                                          \
    X(EXPANSION_FACTOR)                                                        \
    X(INITIAL_RANDOM_SEED)                                                     \
    X(SUBRS)                                                                   \
    X(DEFAULT_WIDTH_X)                                                         \
    X(NOMINAL_WIDTH_X)

#define X(name) CFF_PRIVATE_DICT_##name,
typedef enum { CFF_PRIVATE_DICT_KEYS } CffPrivateDictKey;
#undef X

#define X(name) #name,
static const char* cff_private_dict_key_names[] = {CFF_PRIVATE_DICT_KEYS};
#undef X
#undef CFF_PRIVATE_DICT_KEYS

static PdfError* cff_private_dict_interpret_key(
    CffParser* parser,
    uint8_t operator0,
    CffPrivateDictKey* key_out
) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(operator0 <= 21);
    RELEASE_ASSERT(key_out);

    if (operator0 >= 6 && operator0 <= 9) {
        *key_out = (CffPrivateDictKey)(operator0 - 6);
        return NULL;
    } else if (operator0 == 10 || operator0 == 11) {
        *key_out =
            (CffPrivateDictKey)(operator0 - 10 + CFF_PRIVATE_DICT_STD_HW);
        return NULL;
    } else if (operator0 >= 19 && operator0 <= 21) {
        *key_out = (CffPrivateDictKey)(operator0 - 19 + CFF_PRIVATE_DICT_SUBRS);
        return NULL;
    } else if (operator0 != 12) {
        return PDF_ERROR(PDF_ERR_CFF_INVALID_OPERATOR);
    }

    CffCard8 operator1;
    PDF_PROPAGATE(cff_parser_read_card8(parser, &operator1));

    if (operator1 >= 9 && operator1 <= 11) {
        *key_out =
            (CffPrivateDictKey)(operator1 - 9 + CFF_PRIVATE_DICT_BLUE_SCALE);
        return NULL;
    } else if (operator1 >= 12 && operator1 <= 19) {
        *key_out =
            (CffPrivateDictKey)(operator1 - 12 + CFF_PRIVATE_DICT_STEM_SNAP_H);
        return NULL;
    }

    return PDF_ERROR(PDF_ERR_CFF_INVALID_OPERATOR);
}

static PdfError* read_delta_array(
    Arena* arena,
    CffToken* operand_stack,
    size_t* operand_count,
    Int32Array** array_out
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(operand_stack);
    RELEASE_ASSERT(operand_count);
    RELEASE_ASSERT(array_out);

    *array_out = int32_array_new(arena, *operand_count);

    int32_t prev = 0;
    for (size_t idx = 0; idx < *operand_count; idx++) {
        if (operand_stack[idx].type != CFF_TOKEN_INT_OPERAND) {
            // TODO: can these be reals as well for some keys?
            return PDF_ERROR(PDF_ERR_CFF_INCORRECT_OPERAND);
        }

        int32_t value = prev + operand_stack[idx].value.integer;
        int32_array_set(*array_out, idx, value);
        prev = value;
    }

    *operand_count = 0;

    return NULL;
}

static PdfError*
pop_int(CffToken* operand_stack, size_t* operand_count, int32_t* int_out) {
    RELEASE_ASSERT(operand_stack);
    RELEASE_ASSERT(operand_count);
    RELEASE_ASSERT(int_out);

    if (*operand_count == 0) {
        return PDF_ERROR(PDF_ERR_CFF_MISSING_OPERAND);
    }

    CffToken token = operand_stack[--*operand_count];
    if (token.type != CFF_TOKEN_INT_OPERAND) {
        return PDF_ERROR(
            PDF_ERR_CFF_INCORRECT_OPERAND,
            "Expected integer operand"
        );
    }

    *int_out = token.value.integer;

    return NULL;
}

static PdfError* pop_number(
    CffToken* operand_stack,
    size_t* operand_count,
    CffNumber* number_out
) {
    RELEASE_ASSERT(operand_stack);
    RELEASE_ASSERT(operand_count);
    RELEASE_ASSERT(number_out);

    if (*operand_count == 0) {
        return PDF_ERROR(PDF_ERR_CFF_MISSING_OPERAND);
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
            return PDF_ERROR(
                PDF_ERR_CFF_INCORRECT_OPERAND,
                "Expected number operand"
            );
        }
    }

    if (token.type != CFF_TOKEN_INT_OPERAND
        && token.type != CFF_TOKEN_REAL_OPERAND) {
        return PDF_ERROR(
            PDF_ERR_CFF_INCORRECT_OPERAND,
            "Expected number operand"
        );
    }

    return NULL;
}

PdfError* cff_parse_private_dict(
    Arena* arena,
    CffParser* parser,
    size_t length,
    CffPrivateDict* private_dict_out
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(private_dict_out);

    size_t operand_count = 0;
    CffToken operand_stack[CFF_MAX_OPERANDS] = {0};
    size_t end_offset = parser->offset + length;

    while (parser->offset < end_offset) {
        CffToken token;
        PDF_PROPAGATE(cff_parser_read_token(parser, &token));

        switch (token.type) {
            case CFF_TOKEN_OPERATOR: {
                CffPrivateDictKey key;
                PDF_PROPAGATE(cff_private_dict_interpret_key(
                    parser,
                    token.value.operator,
                    &key
                ));

                LOG_DIAG(
                    DEBUG,
                    CFF,
                    "Key: %s",
                    cff_private_dict_key_names[key]
                );
                switch (key) {
                    case CFF_PRIVATE_DICT_BLUE_VALUES: {
                        PDF_PROPAGATE(read_delta_array(
                            arena,
                            operand_stack,
                            &operand_count,
                            &private_dict_out->blue_values
                        ));
                        break;
                    }
                    case CFF_PRIVATE_DICT_STEM_SNAP_H: {
                        PDF_PROPAGATE(read_delta_array(
                            arena,
                            operand_stack,
                            &operand_count,
                            &private_dict_out->stem_snap_h
                        ));
                        break;
                    }
                    case CFF_PRIVATE_DICT_STEM_SNAP_V: {
                        PDF_PROPAGATE(read_delta_array(
                            arena,
                            operand_stack,
                            &operand_count,
                            &private_dict_out->stem_snap_v
                        ));
                        break;
                    }
                    case CFF_PRIVATE_DICT_STD_HW: {
                        PDF_PROPAGATE(pop_number(
                            operand_stack,
                            &operand_count,
                            &private_dict_out->std_hw
                        ));
                        break;
                    }
                    case CFF_PRIVATE_DICT_STD_VW: {
                        PDF_PROPAGATE(pop_number(
                            operand_stack,
                            &operand_count,
                            &private_dict_out->std_vw
                        ));
                        break;
                    }
                    case CFF_PRIVATE_DICT_SUBRS: {
                        PDF_PROPAGATE(pop_int(
                            operand_stack,
                            &operand_count,
                            &private_dict_out->subrs
                        ));
                        break;
                    }
                    case CFF_PRIVATE_DICT_DEFAULT_WIDTH_X: {
                        PDF_PROPAGATE(pop_number(
                            operand_stack,
                            &operand_count,
                            &private_dict_out->default_width_x
                        ));
                        break;
                    }
                    case CFF_PRIVATE_DICT_NOMINAL_WIDTH_X: {
                        PDF_PROPAGATE(pop_number(
                            operand_stack,
                            &operand_count,
                            &private_dict_out->nominal_width_x
                        ));
                        break;
                    }
                    default: {
                        LOG_TODO("Key `%s`", cff_private_dict_key_names[key]);
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
                    return PDF_ERROR(
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

CffPrivateDict cff_private_dict_default(void) {
    return (CffPrivateDict) {
        .blue_scale = {.type = CFF_NUMBER_REAL, .value.real = 0.039625},
        .blue_shift = { .type = CFF_NUMBER_INT,     .value.integer = 7},
        .blue_fuzz = { .type = CFF_NUMBER_INT,     .value.integer = 1},
        .force_bold = false,
        .language_group = 0,
        .expansion_factor = {.type = CFF_NUMBER_REAL,     .value.real = 0.06},
        .initial_random_seed = 0,
        .default_width_x = { .type = CFF_NUMBER_INT,     .value.integer = 0},
        .nominal_width_x = { .type = CFF_NUMBER_INT,     .value.integer = 0}
    };
}
