#include "operator.h"

#include "log.h"

PdfResult two_char_operator(
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
        return PDF_ERR_UNKNOWN_OPERATOR;
    }

    *selected = operator;
    return PDF_OK;
}

PdfResult select_one_or_two_char_operator(
    PdfCtx* ctx,
    PdfOperator single_char_operator,
    PdfOperator two_char_operator,
    char second_char,
    PdfOperator* selected
) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(selected);

    char peeked;
    PdfResult result = pdf_ctx_peek(ctx, &peeked);

    if (result == PDF_ERR_CTX_EOF
        || (result == PDF_OK && is_pdf_non_regular(peeked))) {
        *selected = single_char_operator;
        return PDF_OK;
    }

    if (result == PDF_OK && peeked == second_char) {
        *selected = two_char_operator;
        return PDF_OK;
    }

    return PDF_ERR_UNKNOWN_OPERATOR;
}

PdfResult
is_single_char_operator(PdfCtx* ctx, bool* is_single_char, char* next_char) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(is_single_char);
    RELEASE_ASSERT(next_char);

    PdfResult result = pdf_ctx_peek(ctx, next_char);

    if (result == PDF_ERR_CTX_EOF
        || (result == PDF_OK && is_pdf_non_regular(*next_char))) {
        *is_single_char = true;
        return PDF_OK;
    }

    if (result != PDF_OK) {
        return result;
    }

    *is_single_char = false;
    PDF_PROPAGATE(pdf_ctx_shift(ctx, 1));
    return PDF_OK;
}

PdfResult pdf_parse_operator(PdfCtx* ctx, PdfOperator* operator) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(operator);

    char peeked;
    PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &peeked));

    switch (peeked) {
        case 'w': {
            *operator= PDF_OPERATOR_w;
            return PDF_OK;
        }
        case 'J': {
            *operator= PDF_OPERATOR_J;
            return PDF_OK;
        }
        case 'j': {
            *operator= PDF_OPERATOR_j;
            return PDF_OK;
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
                return PDF_OK;
            }

            switch (next_char) {
                case '0': {
                    *operator= PDF_OPERATOR_d0;
                    return PDF_OK;
                }
                case '1': {
                    *operator= PDF_OPERATOR_d1;
                    return PDF_OK;
                }
                default: {
                    return PDF_ERR_UNKNOWN_OPERATOR;
                }
            }
        }
        case 'r': {
            char next_char;
            PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &next_char));

            switch (next_char) {
                case 'i': {
                    *operator= PDF_OPERATOR_ri;
                    return PDF_OK;
                }
                case 'e': {
                    *operator= PDF_OPERATOR_re;
                    return PDF_OK;
                }
                case 'g': {
                    *operator= PDF_OPERATOR_rg;
                    return PDF_OK;
                }
                default: {
                    return PDF_ERR_UNKNOWN_OPERATOR;
                }
            }
        }
        case 'i': {
            *operator= PDF_OPERATOR_i;
            return PDF_OK;
        }
        case 'g': {
            return select_one_or_two_char_operator(ctx, PDF_OPERATOR_g, PDF_OPERATOR_gs, 's', operator);
        }
        case 'q': {
            *operator= PDF_OPERATOR_q;
            return PDF_OK;
        }
        case 'Q': {
            *operator= PDF_OPERATOR_Q;
            return PDF_OK;
        }
        case 'c': {
            bool is_single_char;
            char next_char;
            PDF_PROPAGATE(
                is_single_char_operator(ctx, &is_single_char, &next_char)
            );

            if (is_single_char) {
                *operator= PDF_OPERATOR_c;
                return PDF_OK;
            }

            switch (next_char) {
                case 'm': {
                    *operator= PDF_OPERATOR_cm;
                    return PDF_OK;
                }
                case 's': {
                    *operator= PDF_OPERATOR_cs;
                    return PDF_OK;
                }
                default: {
                    return PDF_ERR_UNKNOWN_OPERATOR;
                }
            }
        }
        case 'm': {
            *operator= PDF_OPERATOR_m;
            return PDF_OK;
        }
        case 'l': {
            *operator= PDF_OPERATOR_l;
            return PDF_OK;
        }
        case 'v': {
            *operator= PDF_OPERATOR_v;
            return PDF_OK;
        }
        case 'y': {
            *operator= PDF_OPERATOR_y;
            return PDF_OK;
        }
        case 'h': {
            *operator= PDF_OPERATOR_h;
            return PDF_OK;
        }
        case 'S': {
            bool is_single_char;
            char next_char;
            PDF_PROPAGATE(
                is_single_char_operator(ctx, &is_single_char, &next_char)
            );

            if (is_single_char) {
                *operator= PDF_OPERATOR_S;
                return PDF_OK;
            }

            if (next_char != 'C') {
                return PDF_ERR_UNKNOWN_OPERATOR;
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
                return PDF_OK;
            }

            switch (next_char) {
                case 'c': {
                    return select_one_or_two_char_operator(ctx, PDF_OPERATOR_sc, PDF_OPERATOR_scn, 'n', operator);
                }
                case 'h': {
                    *operator= PDF_OPERATOR_sh;
                    return PDF_OK;
                }
                default: {
                    return PDF_ERR_UNKNOWN_OPERATOR;
                }
            }
        }
        case 'f': {
            return select_one_or_two_char_operator(ctx, PDF_OPERATOR_f, PDF_OPERATOR_f_star, '*', operator);
        }
        case 'F': {
            *operator= PDF_OPERATOR_F;
            return PDF_OK;
        }
        case 'B': {
            char next_char;
            bool is_single_char;
            PDF_PROPAGATE(
                is_single_char_operator(ctx, &is_single_char, &next_char)
            );

            if (is_single_char) {
                *operator= PDF_OPERATOR_B;
                return PDF_OK;
            }

            switch (next_char) {
                case '*': {
                    *operator= PDF_OPERATOR_B_star;
                    return PDF_OK;
                }
                case 'T': {
                    *operator= PDF_OPERATOR_BT;
                    return PDF_OK;
                }
                case 'I': {
                    *operator= PDF_OPERATOR_BI;
                    return PDF_OK;
                }
                case 'M': {
                    return two_char_operator(ctx, PDF_OPERATOR_BMC, 'C', operator);
                }
                case 'D': {
                    return two_char_operator(ctx, PDF_OPERATOR_BDC, 'D', operator);
                }
                case 'X': {
                    *operator= PDF_OPERATOR_BX;
                    return PDF_OK;
                }
                default: {
                    return PDF_ERR_UNKNOWN_OPERATOR;
                }
            }
        }
        case 'b': {
            return select_one_or_two_char_operator(ctx, PDF_OPERATOR_b, PDF_OPERATOR_b_star, '*', operator);
        }
        case 'n': {
            *operator= PDF_OPERATOR_n;
            return PDF_OK;
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
                    return PDF_OK;
                }
                case 'I': {
                    *operator= PDF_OPERATOR_EI;
                    return PDF_OK;
                }
                case 'M': {
                    return two_char_operator(ctx, PDF_OPERATOR_EMC, 'C', operator);
                }
                case 'X': {
                    *operator= PDF_OPERATOR_EX;
                    return PDF_OK;
                }
                default: {
                    return PDF_ERR_UNKNOWN_OPERATOR;
                }
            }
        }
        case 'T': {
            char next_char;
            PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &next_char));

            switch (next_char) {
                case 'c': {
                    *operator= PDF_OPERATOR_Tc;
                    return PDF_OK;
                }
                case 'w': {
                    *operator= PDF_OPERATOR_Tw;
                    return PDF_OK;
                }
                case 'z': {
                    *operator= PDF_OPERATOR_Tz;
                    return PDF_OK;
                }
                case 'L': {
                    *operator= PDF_OPERATOR_TL;
                    return PDF_OK;
                }
                case 'f': {
                    *operator= PDF_OPERATOR_Tf;
                    return PDF_OK;
                }
                case 'r': {
                    *operator= PDF_OPERATOR_Tr;
                    return PDF_OK;
                }
                case 's': {
                    *operator= PDF_OPERATOR_Ts;
                    return PDF_OK;
                }
                case 'd': {
                    *operator= PDF_OPERATOR_Td;
                    return PDF_OK;
                }
                case 'D': {
                    *operator= PDF_OPERATOR_TD;
                    return PDF_OK;
                }
                case 'm': {
                    *operator= PDF_OPERATOR_Tm;
                    return PDF_OK;
                }
                case '*': {
                    *operator= PDF_OPERATOR_T_star;
                    return PDF_OK;
                }
                case 'j': {
                    *operator= PDF_OPERATOR_Tj;
                    return PDF_OK;
                }
                case 'J': {
                    *operator= PDF_OPERATOR_TJ;
                    return PDF_OK;
                }
                default: {
                    return PDF_ERR_UNKNOWN_OPERATOR;
                }
            }
        }
        case '\'': {
            *operator= PDF_OPERATOR_single_quote;
            return PDF_OK;
        }
        case '"': {
            *operator= PDF_OPERATOR_double_quote;
            return PDF_OK;
        }
        case 'C': {
            return two_char_operator(ctx, PDF_OPERATOR_CS, 'S', operator);
        }
        case 'G': {
            *operator= PDF_OPERATOR_G;
            return PDF_OK;
        }
        case 'R': {
            return two_char_operator(ctx, PDF_OPERATOR_RG, 'G', operator);
        }
        case 'K': {
            *operator= PDF_OPERATOR_K;
            return PDF_OK;
        }
        case 'k': {
            *operator= PDF_OPERATOR_k;
            return PDF_OK;
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
                    return PDF_OK;
                }
                case 'P': {
                    *operator= PDF_OPERATOR_DP;
                    return PDF_OK;
                }
                default: {
                    return PDF_ERR_UNKNOWN_OPERATOR;
                }
            }
        }
        default: {
            return PDF_ERR_UNKNOWN_OPERATOR;
        }
    }
}
