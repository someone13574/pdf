#include "operator.h"

#include "log.h"
#include "pdf/error.h"

PdfError* two_char_operator(
    PdfCtx* ctx,
    PdfOperator
    operator,
    char second_char,
    PdfOperator* selected
) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(selected);

    char peeked;
    PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &peeked));

    if (peeked != second_char) {
        return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
    }

    *selected = operator;
    return NULL;
}

PdfError* select_one_or_two_char_operator(
    PdfCtx* ctx,
    PdfOperator single_char_operator,
    PdfOperator two_char_operator,
    char second_char,
    PdfOperator* selected
) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(selected);

    char peeked;
    PdfError* error = pdf_ctx_peek(ctx, &peeked);

    if ((error && pdf_error_code(error) == PDF_ERR_CTX_EOF)
        || (!error && is_pdf_non_regular(peeked))) {
        *selected = single_char_operator;
        if (error) {
            pdf_error_free(error);
        }
        return NULL;
    }

    if (pdf_error_free_is_ok(error) && peeked == second_char) {
        *selected = two_char_operator;
        return NULL;
    }

    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
}

PdfError*
is_single_char_operator(PdfCtx* ctx, bool* is_single_char, char* next_char) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(is_single_char);
    RELEASE_ASSERT(next_char);

    PdfError* error = pdf_ctx_peek(ctx, next_char);
    if ((error && pdf_error_code(error) == PDF_ERR_CTX_EOF)
        || (!error && is_pdf_non_regular(*next_char))) {
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

    char peeked;
    PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &peeked));

    switch (peeked) {
        case 'w': {
            *operator= PDF_OPERATOR_w;
            return NULL;
        }
        case 'J': {
            *operator= PDF_OPERATOR_J;
            return NULL;
        }
        case 'j': {
            *operator= PDF_OPERATOR_j;
            return NULL;
        }
        case 'M': {
            return select_one_or_two_char_operator(ctx, PDF_OPERATOR_M, PDF_OPERATOR_MP, 'P', operator);
        }
        case 'd': {
            bool is_single_char;
            char next_char;
            PDF_PROPAGATE(
                is_single_char_operator(ctx, &is_single_char, &next_char)
            );

            if (is_single_char) {
                *operator= PDF_OPERATOR_d;
                return NULL;
            }

            switch (next_char) {
                case '0': {
                    *operator= PDF_OPERATOR_d0;
                    return NULL;
                }
                case '1': {
                    *operator= PDF_OPERATOR_d1;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case 'r': {
            char next_char;
            PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &next_char));

            switch (next_char) {
                case 'i': {
                    *operator= PDF_OPERATOR_ri;
                    return NULL;
                }
                case 'e': {
                    *operator= PDF_OPERATOR_re;
                    return NULL;
                }
                case 'g': {
                    *operator= PDF_OPERATOR_rg;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case 'i': {
            *operator= PDF_OPERATOR_i;
            return NULL;
        }
        case 'g': {
            return select_one_or_two_char_operator(ctx, PDF_OPERATOR_g, PDF_OPERATOR_gs, 's', operator);
        }
        case 'q': {
            *operator= PDF_OPERATOR_q;
            return NULL;
        }
        case 'Q': {
            *operator= PDF_OPERATOR_Q;
            return NULL;
        }
        case 'c': {
            bool is_single_char;
            char next_char;
            PDF_PROPAGATE(
                is_single_char_operator(ctx, &is_single_char, &next_char)
            );

            if (is_single_char) {
                *operator= PDF_OPERATOR_c;
                return NULL;
            }

            switch (next_char) {
                case 'm': {
                    *operator= PDF_OPERATOR_cm;
                    return NULL;
                }
                case 's': {
                    *operator= PDF_OPERATOR_cs;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case 'm': {
            *operator= PDF_OPERATOR_m;
            return NULL;
        }
        case 'l': {
            *operator= PDF_OPERATOR_l;
            return NULL;
        }
        case 'v': {
            *operator= PDF_OPERATOR_v;
            return NULL;
        }
        case 'y': {
            *operator= PDF_OPERATOR_y;
            return NULL;
        }
        case 'h': {
            *operator= PDF_OPERATOR_h;
            return NULL;
        }
        case 'S': {
            bool is_single_char;
            char next_char;
            PDF_PROPAGATE(
                is_single_char_operator(ctx, &is_single_char, &next_char)
            );

            if (is_single_char) {
                *operator= PDF_OPERATOR_S;
                return NULL;
            }

            if (next_char != 'C') {
                return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
            }

            return select_one_or_two_char_operator(ctx, PDF_OPERATOR_SC, PDF_OPERATOR_SCN, 'N', operator);
        }
        case 's': {
            bool is_single_char;
            char next_char;
            PDF_PROPAGATE(
                is_single_char_operator(ctx, &is_single_char, &next_char)
            );

            if (is_single_char) {
                *operator= PDF_OPERATOR_S;
                return NULL;
            }

            switch (next_char) {
                case 'c': {
                    return select_one_or_two_char_operator(ctx, PDF_OPERATOR_sc, PDF_OPERATOR_scn, 'n', operator);
                }
                case 'h': {
                    *operator= PDF_OPERATOR_sh;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case 'f': {
            return select_one_or_two_char_operator(ctx, PDF_OPERATOR_f, PDF_OPERATOR_f_star, '*', operator);
        }
        case 'F': {
            *operator= PDF_OPERATOR_F;
            return NULL;
        }
        case 'B': {
            char next_char;
            bool is_single_char;
            PDF_PROPAGATE(
                is_single_char_operator(ctx, &is_single_char, &next_char)
            );

            if (is_single_char) {
                *operator= PDF_OPERATOR_B;
                return NULL;
            }

            switch (next_char) {
                case '*': {
                    *operator= PDF_OPERATOR_B_star;
                    return NULL;
                }
                case 'T': {
                    *operator= PDF_OPERATOR_BT;
                    return NULL;
                }
                case 'I': {
                    *operator= PDF_OPERATOR_BI;
                    return NULL;
                }
                case 'M': {
                    return two_char_operator(ctx, PDF_OPERATOR_BMC, 'C', operator);
                }
                case 'D': {
                    return two_char_operator(ctx, PDF_OPERATOR_BDC, 'D', operator);
                }
                case 'X': {
                    *operator= PDF_OPERATOR_BX;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case 'b': {
            return select_one_or_two_char_operator(ctx, PDF_OPERATOR_b, PDF_OPERATOR_b_star, '*', operator);
        }
        case 'n': {
            *operator= PDF_OPERATOR_n;
            return NULL;
        }
        case 'W': {
            return select_one_or_two_char_operator(ctx, PDF_OPERATOR_W, PDF_OPERATOR_W_star, '*', operator);
        }
        case 'E': {
            char next_char;
            PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &next_char));

            switch (next_char) {
                case 'T': {
                    *operator= PDF_OPERATOR_ET;
                    return NULL;
                }
                case 'I': {
                    *operator= PDF_OPERATOR_EI;
                    return NULL;
                }
                case 'M': {
                    return two_char_operator(ctx, PDF_OPERATOR_EMC, 'C', operator);
                }
                case 'X': {
                    *operator= PDF_OPERATOR_EX;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case 'T': {
            char next_char;
            PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &next_char));

            switch (next_char) {
                case 'c': {
                    *operator= PDF_OPERATOR_Tc;
                    return NULL;
                }
                case 'w': {
                    *operator= PDF_OPERATOR_Tw;
                    return NULL;
                }
                case 'z': {
                    *operator= PDF_OPERATOR_Tz;
                    return NULL;
                }
                case 'L': {
                    *operator= PDF_OPERATOR_TL;
                    return NULL;
                }
                case 'f': {
                    *operator= PDF_OPERATOR_Tf;
                    return NULL;
                }
                case 'r': {
                    *operator= PDF_OPERATOR_Tr;
                    return NULL;
                }
                case 's': {
                    *operator= PDF_OPERATOR_Ts;
                    return NULL;
                }
                case 'd': {
                    *operator= PDF_OPERATOR_Td;
                    return NULL;
                }
                case 'D': {
                    *operator= PDF_OPERATOR_TD;
                    return NULL;
                }
                case 'm': {
                    *operator= PDF_OPERATOR_Tm;
                    return NULL;
                }
                case '*': {
                    *operator= PDF_OPERATOR_T_star;
                    return NULL;
                }
                case 'j': {
                    *operator= PDF_OPERATOR_Tj;
                    return NULL;
                }
                case 'J': {
                    *operator= PDF_OPERATOR_TJ;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        case '\'': {
            *operator= PDF_OPERATOR_single_quote;
            return NULL;
        }
        case '"': {
            *operator= PDF_OPERATOR_double_quote;
            return NULL;
        }
        case 'C': {
            return two_char_operator(ctx, PDF_OPERATOR_CS, 'S', operator);
        }
        case 'G': {
            *operator= PDF_OPERATOR_G;
            return NULL;
        }
        case 'R': {
            return two_char_operator(ctx, PDF_OPERATOR_RG, 'G', operator);
        }
        case 'K': {
            *operator= PDF_OPERATOR_K;
            return NULL;
        }
        case 'k': {
            *operator= PDF_OPERATOR_k;
            return NULL;
        }
        case 'I': {
            return two_char_operator(ctx, PDF_OPERATOR_ID, 'D', operator);
        }
        case 'D': {
            char next_char;
            PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &next_char));

            switch (next_char) {
                case 'o': {
                    *operator= PDF_OPERATOR_Do;
                    return NULL;
                }
                case 'P': {
                    *operator= PDF_OPERATOR_DP;
                    return NULL;
                }
                default: {
                    return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
                }
            }
        }
        default: {
            return PDF_ERROR(PDF_ERR_UNKNOWN_OPERATOR);
        }
    }
}
