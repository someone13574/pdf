#include "operator.h"

#include <stdint.h>

#include "logger/log.h"
#include "pdf_error/error.h"

static PdfError* two_byte_operator(
    PdfCtx* ctx,
    PdfOperator operator,
    uint8_t second_byte,
    PdfOperator* selected
) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(selected);

    uint8_t peeked;
    PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &peeked));

    if (peeked != second_byte) {
        return PDF_ERROR(
            PDF_ERR_UNKNOWN_OPERATOR,
            "Expected char `%c`, found `%c`",
            second_byte,
            peeked
        );
    }

    *selected = operator;
    return NULL;
}

static PdfError* select_one_or_two_byte_operator(
    PdfCtx* ctx,
    PdfOperator single_byte_operator,
    PdfOperator two_byte_operator,
    uint8_t second_byte,
    PdfOperator* selected
) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(selected);

    uint8_t peeked;
    PdfError* error = pdf_ctx_peek(ctx, &peeked);

    if ((error && pdf_error_code(error) == PDF_ERR_CTX_EOF)
        || (!error && is_pdf_non_regular(peeked))) {
        *selected = single_byte_operator;
        if (error) {
            pdf_error_free(error);
        }
        return NULL;
    }

    if (pdf_error_free_is_ok(error) && peeked == second_byte) {
        *selected = two_byte_operator;
        PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, NULL));
        return NULL;
    }

    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
}

static PdfError*
is_single_byte_operator(PdfCtx* ctx, bool* is_single_char, uint8_t* next_byte) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(is_single_char);
    RELEASE_ASSERT(next_byte);

    PdfError* error = pdf_ctx_peek(ctx, next_byte);
    if ((error && pdf_error_code(error) == PDF_ERR_CTX_EOF)
        || (!error && is_pdf_non_regular(*next_byte))) {
        *is_single_char = true;
        if (error) {
            pdf_error_free(error);
        }
        return NULL;
    }

    if (error) {
        return error;
    }

    *is_single_char = false;
    PDF_PROPAGATE(pdf_ctx_shift(ctx, 1));
    return NULL;
}

PdfError* pdf_parse_operator(PdfCtx* ctx, PdfOperator* operator) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(operator);

    uint8_t peeked;
    PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &peeked));

    switch (peeked) {
        case 'w': {
            *operator = PDF_OPERATOR_w;
            return NULL;
        }
        case 'J': {
            *operator = PDF_OPERATOR_J;
            return NULL;
        }
        case 'j': {
            *operator = PDF_OPERATOR_j;
            return NULL;
        }
        case 'M': {
            return select_one_or_two_byte_operator(
                ctx,
                PDF_OPERATOR_M,
                PDF_OPERATOR_MP,
                'P',
                operator
            );
        }
        case 'd': {
            bool is_single_byte;
            uint8_t next_byte;
            PDF_PROPAGATE(
                is_single_byte_operator(ctx, &is_single_byte, &next_byte)
            );

            if (is_single_byte) {
                *operator = PDF_OPERATOR_d;
                return NULL;
            }

            switch (next_byte) {
                case '0': {
                    *operator = PDF_OPERATOR_d0;
                    return NULL;
                }
                case '1': {
                    *operator = PDF_OPERATOR_d1;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case 'r': {
            uint8_t next_byte;
            PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &next_byte));

            switch (next_byte) {
                case 'i': {
                    *operator = PDF_OPERATOR_ri;
                    return NULL;
                }
                case 'e': {
                    *operator = PDF_OPERATOR_re;
                    return NULL;
                }
                case 'g': {
                    *operator = PDF_OPERATOR_rg;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case 'i': {
            *operator = PDF_OPERATOR_i;
            return NULL;
        }
        case 'g': {
            return select_one_or_two_byte_operator(
                ctx,
                PDF_OPERATOR_g,
                PDF_OPERATOR_gs,
                's',
                operator
            );
        }
        case 'q': {
            *operator = PDF_OPERATOR_q;
            return NULL;
        }
        case 'Q': {
            *operator = PDF_OPERATOR_Q;
            return NULL;
        }
        case 'c': {
            bool is_single_byte;
            uint8_t next_byte;
            PDF_PROPAGATE(
                is_single_byte_operator(ctx, &is_single_byte, &next_byte)
            );

            if (is_single_byte) {
                *operator = PDF_OPERATOR_c;
                return NULL;
            }

            switch (next_byte) {
                case 'm': {
                    *operator = PDF_OPERATOR_cm;
                    return NULL;
                }
                case 's': {
                    *operator = PDF_OPERATOR_cs;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case 'm': {
            *operator = PDF_OPERATOR_m;
            return NULL;
        }
        case 'l': {
            *operator = PDF_OPERATOR_l;
            return NULL;
        }
        case 'v': {
            *operator = PDF_OPERATOR_v;
            return NULL;
        }
        case 'y': {
            *operator = PDF_OPERATOR_y;
            return NULL;
        }
        case 'h': {
            *operator = PDF_OPERATOR_h;
            return NULL;
        }
        case 'S': {
            bool is_single_byte;
            uint8_t next_byte;
            PDF_PROPAGATE(
                is_single_byte_operator(ctx, &is_single_byte, &next_byte)
            );

            if (is_single_byte) {
                *operator = PDF_OPERATOR_S;
                return NULL;
            }

            if (next_byte != 'C') {
                return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
            }

            return select_one_or_two_byte_operator(
                ctx,
                PDF_OPERATOR_SC,
                PDF_OPERATOR_SCN,
                'N',
                operator
            );
        }
        case 's': {
            bool is_single_byte;
            uint8_t next_byte;
            PDF_PROPAGATE(
                is_single_byte_operator(ctx, &is_single_byte, &next_byte)
            );

            if (is_single_byte) {
                *operator = PDF_OPERATOR_S;
                return NULL;
            }

            switch (next_byte) {
                case 'c': {
                    return select_one_or_two_byte_operator(
                        ctx,
                        PDF_OPERATOR_sc,
                        PDF_OPERATOR_scn,
                        'n',
                        operator
                    );
                }
                case 'h': {
                    *operator = PDF_OPERATOR_sh;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case 'f': {
            return select_one_or_two_byte_operator(
                ctx,
                PDF_OPERATOR_f,
                PDF_OPERATOR_f_star,
                '*',
                operator
            );
        }
        case 'F': {
            *operator = PDF_OPERATOR_F;
            return NULL;
        }
        case 'B': {
            bool is_single_byte;
            uint8_t next_byte;
            PDF_PROPAGATE(
                is_single_byte_operator(ctx, &is_single_byte, &next_byte)
            );

            if (is_single_byte) {
                *operator = PDF_OPERATOR_B;
                return NULL;
            }

            switch (next_byte) {
                case '*': {
                    *operator = PDF_OPERATOR_B_star;
                    return NULL;
                }
                case 'T': {
                    *operator = PDF_OPERATOR_BT;
                    return NULL;
                }
                case 'I': {
                    *operator = PDF_OPERATOR_BI;
                    return NULL;
                }
                case 'M': {
                    return two_byte_operator(
                        ctx,
                        PDF_OPERATOR_BMC,
                        'C',
                        operator
                    );
                }
                case 'D': {
                    return two_byte_operator(
                        ctx,
                        PDF_OPERATOR_BDC,
                        'C',
                        operator
                    );
                }
                case 'X': {
                    *operator = PDF_OPERATOR_BX;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case 'b': {
            return select_one_or_two_byte_operator(
                ctx,
                PDF_OPERATOR_b,
                PDF_OPERATOR_b_star,
                '*',
                operator
            );
        }
        case 'n': {
            *operator = PDF_OPERATOR_n;
            return NULL;
        }
        case 'W': {
            return select_one_or_two_byte_operator(
                ctx,
                PDF_OPERATOR_W,
                PDF_OPERATOR_W_star,
                '*',
                operator
            );
        }
        case 'E': {
            uint8_t next_byte;
            PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &next_byte));

            switch (next_byte) {
                case 'T': {
                    *operator = PDF_OPERATOR_ET;
                    return NULL;
                }
                case 'I': {
                    *operator = PDF_OPERATOR_EI;
                    return NULL;
                }
                case 'M': {
                    return two_byte_operator(
                        ctx,
                        PDF_OPERATOR_EMC,
                        'C',
                        operator
                    );
                }
                case 'X': {
                    *operator = PDF_OPERATOR_EX;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case 'T': {
            uint8_t next_byte;
            PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &next_byte));

            switch (next_byte) {
                case 'c': {
                    *operator = PDF_OPERATOR_Tc;
                    return NULL;
                }
                case 'w': {
                    *operator = PDF_OPERATOR_Tw;
                    return NULL;
                }
                case 'z': {
                    *operator = PDF_OPERATOR_Tz;
                    return NULL;
                }
                case 'L': {
                    *operator = PDF_OPERATOR_TL;
                    return NULL;
                }
                case 'f': {
                    *operator = PDF_OPERATOR_Tf;
                    return NULL;
                }
                case 'r': {
                    *operator = PDF_OPERATOR_Tr;
                    return NULL;
                }
                case 's': {
                    *operator = PDF_OPERATOR_Ts;
                    return NULL;
                }
                case 'd': {
                    *operator = PDF_OPERATOR_Td;
                    return NULL;
                }
                case 'D': {
                    *operator = PDF_OPERATOR_TD;
                    return NULL;
                }
                case 'm': {
                    *operator = PDF_OPERATOR_Tm;
                    return NULL;
                }
                case '*': {
                    *operator = PDF_OPERATOR_T_star;
                    return NULL;
                }
                case 'j': {
                    *operator = PDF_OPERATOR_Tj;
                    return NULL;
                }
                case 'J': {
                    *operator = PDF_OPERATOR_TJ;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case '\'': {
            *operator = PDF_OPERATOR_single_quote;
            return NULL;
        }
        case '"': {
            *operator = PDF_OPERATOR_double_quote;
            return NULL;
        }
        case 'C': {
            return two_byte_operator(ctx, PDF_OPERATOR_CS, 'S', operator);
        }
        case 'G': {
            *operator = PDF_OPERATOR_G;
            return NULL;
        }
        case 'R': {
            return two_byte_operator(ctx, PDF_OPERATOR_RG, 'G', operator);
        }
        case 'K': {
            *operator = PDF_OPERATOR_K;
            return NULL;
        }
        case 'k': {
            *operator = PDF_OPERATOR_k;
            return NULL;
        }
        case 'I': {
            return two_byte_operator(ctx, PDF_OPERATOR_ID, 'D', operator);
        }
        case 'D': {
            uint8_t next_byte;
            PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &next_byte));

            switch (next_byte) {
                case 'o': {
                    *operator = PDF_OPERATOR_Do;
                    return NULL;
                }
                case 'P': {
                    *operator = PDF_OPERATOR_DP;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        default: {
            return PDF_ERROR(
                PDF_ERR_UNKNOWN_OPERATOR,
                "First byte: %c",
                peeked
            );
        }
    }
}
