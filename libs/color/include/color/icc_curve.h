#pragma once

#include <stdint.h>

#include "err/error.h"
#include "parse_ctx/ctx.h"

typedef struct {
    uint32_t signature;
    uint32_t reserved;
    uint32_t entries;
    ParseCtx data;
} IccCurve;

Error* icc_parse_curve(ParseCtx* ctx, IccCurve* out);
Error* icc_curve_map(IccCurve curve, double x, bool is_pcsxyz, double* out);

typedef struct {
    uint32_t signature;
    uint32_t reserved;
    uint16_t function_type;
    uint16_t reserved2;

    double g;
    double a;
    double b;
    double c;
    double d;
    double e;
    double f;
} IccParametricCurve;

Error* icc_parse_parametric_curve(ParseCtx* ctx, IccParametricCurve* out);
double icc_parametric_curve_map(IccParametricCurve curve, double x);

typedef struct {
    bool parametric;
    union {
        IccCurve curve;
        IccParametricCurve parametric;
    } curve;
} IccAnyCurve;

Error* icc_parse_any_curve(ParseCtx* ctx, IccAnyCurve* out);
Error*
icc_any_curve_map(IccAnyCurve curve, bool is_pcsxyz, double x, double* out);

#define DVEC_NAME IccAnyCurveVec
#define DVEC_LOWERCASE_NAME icc_any_curve_vec
#define DVEC_TYPE IccAnyCurve
#include "arena/dvec_decl.h"
