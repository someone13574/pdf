#include "color/icc_curve.h"

#include <math.h>
#include <stdint.h>

#include "color/icc_types.h"
#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/binary.h"
#include "parse_ctx/ctx.h"

Error* icc_parse_curve(ParseCtx* ctx, IccCurve* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);

    TRY(parse_ctx_read_u32_be(ctx, &out->signature));
    TRY(parse_ctx_read_u32_be(ctx, &out->reserved));
    TRY(parse_ctx_read_u32_be(ctx, &out->entries));

    if (out->signature != 0x63757276) {
        return ERROR(ICC_ERR_INVALID_SIGNATURE);
    }

    if (out->reserved != 0) {
        LOG_WARN(ICC, "Reserved field not 0");
    }

    TRY(parse_ctx_new_subctx(ctx, 2 * (size_t)out->entries, &out->data));

    return NULL;
}

Error* icc_curve_map(IccCurve curve, double x, bool is_pcsxyz, double* out) {
    RELEASE_ASSERT(out);

    static const double PCSCYZ_CONSTANT = 1.0 + (32767.0 / 32768.0);
    if (is_pcsxyz && fabs(x - PCSCYZ_CONSTANT) < 1e-6) {
        x = 1.0;
    }

    if (curve.entries == 0) {
        *out = x;
    } else if (curve.entries == 1) {
        IccU8Fixed8Number gamma_raw;
        TRY(parse_ctx_read_u16_be(&curve.data, &gamma_raw));
        *out = pow(x, icc_u8_fixed8_to_double(gamma_raw));
    } else {
        double max_coord = (double)(curve.entries - 1);
        double coord = x * max_coord;
        size_t a = (size_t)fmax(floor(coord), 0.0);
        size_t b = a + 1;
        double t = coord - (double)a;

        a = (a > curve.entries - 1) ? curve.entries - 1 : a;
        b = (b > curve.entries - 1) ? curve.entries - 1 : b;

        uint16_t sample_a, sample_b;
        TRY(parse_ctx_seek(&curve.data, (size_t)a * 2));
        TRY(parse_ctx_read_u16_be(&curve.data, &sample_a));
        TRY(parse_ctx_seek(&curve.data, (size_t)b * 2));
        TRY(parse_ctx_read_u16_be(&curve.data, &sample_b));

        *out = (double)sample_a * (1.0 - t) + (double)sample_b * t;
        *out /= 65535.0;
    }

    if (is_pcsxyz && fabs(*out - 1.0) < 1e-6) {
        *out = PCSCYZ_CONSTANT;
    }

    return NULL;
}

Error* icc_parse_parametric_curve(ParseCtx* ctx, IccParametricCurve* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);

    TRY(parse_ctx_read_u32_be(ctx, &out->signature));
    TRY(parse_ctx_read_u32_be(ctx, &out->reserved));
    TRY(parse_ctx_read_u16_be(ctx, &out->function_type));
    TRY(parse_ctx_read_u16_be(ctx, &out->reserved2));

    if (out->signature != 0x70617261) {
        return ERROR(ICC_ERR_INVALID_SIGNATURE);
    }

    if (out->reserved != 0 || out->reserved2 != 0) {
        LOG_WARN(ICC, "Reserved field not 0");
    }

    if (out->function_type > 4) {
        return ERROR(
            ICC_ERR_UNKNOWN_PARA_FN,
            "Unknown function type %d",
            (int)out->function_type
        );
    }

    IccS15Fixed16Number g = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    TRY(parse_ctx_read_i32_be(ctx, &g));
    if (out->function_type >= 1) {
        TRY(parse_ctx_read_i32_be(ctx, &a));
        TRY(parse_ctx_read_i32_be(ctx, &b));
    }
    if (out->function_type >= 2) {
        TRY(parse_ctx_read_i32_be(ctx, &c));
    }
    if (out->function_type >= 3) {
        TRY(parse_ctx_read_i32_be(ctx, &d));
    }
    if (out->function_type >= 4) {
        TRY(parse_ctx_read_i32_be(ctx, &e));
        TRY(parse_ctx_read_i32_be(ctx, &f));
    }

    out->g = icc_s15_fixed16_to_double(g);
    out->a = icc_s15_fixed16_to_double(a);
    out->b = icc_s15_fixed16_to_double(b);
    out->c = icc_s15_fixed16_to_double(c);
    out->d = icc_s15_fixed16_to_double(d);
    out->e = icc_s15_fixed16_to_double(e);
    out->f = icc_s15_fixed16_to_double(f);

    return NULL;
}

double icc_parametric_curve_map(IccParametricCurve curve, double x) {
    x = fmax(fmin(x, 1.0), 0.0);

    switch (curve.function_type) {
        case 0: {
            x = pow(x, curve.g);
            break;
        }
        case 1: {
            if (x >= -curve.b / curve.a) {
                x = pow(curve.a * x + curve.b, curve.g);
            } else {
                x = 0.0;
            }
            break;
        }
        case 2: {
            if (x >= -curve.b / curve.a) {
                x = pow(curve.a * x + curve.b, curve.g) + curve.c;
            } else {
                x = curve.c;
            }
            break;
        }
        case 3: {
            if (x >= curve.d) {
                x = pow(curve.a * x + curve.b, curve.g);
            } else {
                x = curve.c * x;
            }
            break;
        }
        case 4: {
            if (x >= curve.d) {
                x = pow(curve.a * x + curve.b, curve.g) + curve.e;
            } else {
                x = curve.c * x + curve.f;
            }
            break;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }

    return fmax(fmin(x, 1.0), 0.0);
}

Error* icc_parse_any_curve(ParseCtx* ctx, IccAnyCurve* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);

    uint32_t signature;
    TRY(parse_ctx_read_u32_be(ctx, &signature));
    ctx->offset -= 4;

    switch (signature) {
        case 0x63757276: {
            out->parametric = false;
            TRY(icc_parse_curve(ctx, &out->curve.curve));
            break;
        }
        case 0x70617261: {
            out->parametric = true;
            TRY(icc_parse_parametric_curve(ctx, &out->curve.parametric));
            break;
        }
        default: {
            return ERROR(ICC_ERR_INVALID_SIGNATURE, "Invalid curve signature");
        }
    }

    return NULL;
}

Error*
icc_any_curve_map(IccAnyCurve curve, bool is_pcsxyz, double x, double* out) {
    RELEASE_ASSERT(out);

    LOG_DIAG(TRACE, ICC, "Mapping 1d any: %f", x);
    if (curve.parametric) {
        *out = icc_parametric_curve_map(curve.curve.parametric, x);
    } else {
        TRY(icc_curve_map(curve.curve.curve, x, is_pcsxyz, out));
    }

    LOG_DIAG(TRACE, ICC, "Out: %f", *out);

    return NULL;
}

#define DVEC_NAME IccAnyCurveVec
#define DVEC_LOWERCASE_NAME icc_any_curve_vec
#define DVEC_TYPE IccAnyCurve
#include "arena/dvec_impl.h"
