#include "color/icc_lut.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena/arena.h"
#include "color/icc_color.h"
#include "color/icc_curve.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "geom/vec3.h"
#include "logger/log.h"
#include "parse_ctx/ctx.h"

static uint64_t integer_pow(uint64_t base, uint64_t exponent) {
    uint64_t acc = 1;
    for (uint64_t idx = 0; idx < exponent; idx++) {
        acc *= base;
    }

    return acc;
}

static Error* icc_parse_lut_common(ParseCtx* ctx, IccStandardLutCommon* out) {
    RELEASE_ASSERT(ctx->buffer);
    RELEASE_ASSERT(out);

    TRY(parse_ctx_seek(ctx, 0));
    TRY(parse_ctx_bound_check(ctx, 48));

    TRY(parse_ctx_read_u32_be(ctx, &out->signature));
    TRY(parse_ctx_read_u32_be(ctx, &out->reserved));
    TRY(parse_ctx_read_u8(ctx, &out->input_channels));
    TRY(parse_ctx_read_u8(ctx, &out->output_channels));
    TRY(parse_ctx_read_u8(ctx, &out->grid_points));
    TRY(parse_ctx_read_u8(ctx, &out->padding));
    TRY(parse_ctx_read_i32_be(ctx, &out->e1));
    TRY(parse_ctx_read_i32_be(ctx, &out->e2));
    TRY(parse_ctx_read_i32_be(ctx, &out->e3));
    TRY(parse_ctx_read_i32_be(ctx, &out->e4));
    TRY(parse_ctx_read_i32_be(ctx, &out->e5));
    TRY(parse_ctx_read_i32_be(ctx, &out->e6));
    TRY(parse_ctx_read_i32_be(ctx, &out->e7));
    TRY(parse_ctx_read_i32_be(ctx, &out->e8));
    TRY(parse_ctx_read_i32_be(ctx, &out->e9));

    out->matrix = geom_mat3_new(
        icc_s15_fixed16_to_double(out->e1),
        icc_s15_fixed16_to_double(out->e2),
        icc_s15_fixed16_to_double(out->e3),
        icc_s15_fixed16_to_double(out->e4),
        icc_s15_fixed16_to_double(out->e5),
        icc_s15_fixed16_to_double(out->e6),
        icc_s15_fixed16_to_double(out->e7),
        icc_s15_fixed16_to_double(out->e8),
        icc_s15_fixed16_to_double(out->e9)
    );

    RELEASE_ASSERT(ctx->offset == 48);

    if (out->grid_points < 2) {
        return ERROR(ICC_ERR_INVALID_LUT);
    } else if (out->input_channels > 15 || out->input_channels == 0) {
        return ERROR(ICC_ERR_INVALID_LUT);
    } else if (out->output_channels > 15 || out->output_channels == 0) {
        return ERROR(ICC_ERR_INVALID_LUT);
    }

    return NULL;
}

Error* icc_parse_lut8(ParseCtx ctx, IccLut8* out) {
    RELEASE_ASSERT(ctx.buffer);
    RELEASE_ASSERT(out);

    TRY(icc_parse_lut_common(&ctx, &out->common));
    if (out->common.signature != 0x6D667431) {
        return ERROR(ICC_ERR_INVALID_SIGNATURE);
    }

    TRY(parse_ctx_new_subctx(
        &ctx,
        ctx.offset,
        256 * (size_t)out->common.input_channels,
        &out->input_table
    ));

    TRY(parse_ctx_new_subctx(
        &ctx,
        ctx.offset,
        integer_pow(out->common.grid_points, out->common.input_channels)
            * (size_t)out->common.output_channels,
        &out->clut
    ));

    TRY(parse_ctx_new_subctx(
        &ctx,
        ctx.offset,
        256 * (size_t)out->common.output_channels,
        &out->output_table
    ));

    return NULL;
}

static Error*
icc_lut8_map_1d(ParseCtx table, double val, size_t channel, double* out) {
    val = val < 0.0 ? 0.0 : val;
    val = val > 1.0 ? 1.0 : val;

    double x = val * 255.0 + (double)channel * 256.0;
    double frac = x - floor(x);
    bool interpolate = (size_t)floor(x) < 255 + (size_t)channel * 256;

    uint8_t sample_a;
    TRY(parse_ctx_seek(&table, (size_t)floor(x)));
    TRY(parse_ctx_read_u8(&table, &sample_a));

    if (interpolate) {
        uint8_t sample_b;
        TRY(parse_ctx_read_u8(&table, &sample_b));

        double a = (double)sample_a / 255.0;
        double b = (double)sample_b / 255.0;
        *out = a + frac * (b - a);
    } else {
        *out = (double)sample_a / 255.0;
    }

    return NULL;
}

static Error*
icc_lut8_clut_map(IccLut8 lut, double coords[15], double out[15]) {
    RELEASE_ASSERT(coords);
    RELEASE_ASSERT(out);
    RELEASE_ASSERT(lut.common.input_channels >= 1);

    double max_coord = (double)lut.common.grid_points - 1.0;
    for (size_t idx = 0; idx < lut.common.input_channels; idx++) {
        coords[idx] = coords[idx] < 0.0 ? 0.0 : coords[idx];
        coords[idx] = coords[idx] > 1.0 ? 1.0 : coords[idx];
        coords[idx] *= max_coord;
    }

    size_t strides[15];
    strides[lut.common.input_channels - 1] = 1;
    for (int idx = (int)lut.common.input_channels - 2; idx >= 0; idx--) {
        strides[idx] = strides[idx + 1] * lut.common.grid_points;
    }

    double acc[15] = {0}; // TODO: Kahan summation
    uint16_t max_state = (uint16_t)(1 << lut.common.input_channels) - 1;
    for (uint16_t state = 0; state <= max_state; state++) {
        size_t offset = 0;
        double weight = 1.0;
        for (size_t in_channel = 0; in_channel < lut.common.input_channels;
             in_channel++) {
            bool high = ((state >> in_channel) & 1) == 1;
            double frac = coords[in_channel] - floor(coords[in_channel]);
            weight *= high ? frac : (1.0 - frac);

            double coord = coords[in_channel] + (high ? 1.0 : 0.0);
            coord = coord > max_coord ? max_coord : coord;
            offset += (size_t)coord * strides[in_channel];
        }

        offset *= (size_t)lut.common.output_channels;
        TRY(parse_ctx_seek(&lut.clut, offset));

        for (size_t out_channel = 0; out_channel < lut.common.output_channels;
             out_channel++) {
            uint8_t sample;
            TRY(parse_ctx_read_u8(&lut.clut, &sample));
            acc[out_channel] += (double)sample * weight;
        }
    }

    for (size_t output = 0; output < lut.common.output_channels; output++) {
        out[output] = acc[output] / 255.0;
    }

    return NULL;
}

Error* icc_lut8_map(IccLut8 lut, IccColor input, double out[15]) {
    RELEASE_ASSERT(out);

    LOG_DIAG(DEBUG, ICC, "Applying lut8");

    if (icc_color_space_channels(input.color_space)
        != lut.common.input_channels) {
        return ERROR(
            ICC_ERR_INCORRECT_CHANNELS,
            "Input space %d has %d channels, but the lut has %d",
            (int)input.color_space,
            (int)icc_color_space_channels(input.color_space),
            (int)lut.common.input_channels
        );
    }

    icc_color_norm_pcs(&input, lut.common.matrix);

    double temp[15] = {0};
    for (size_t channel = 0; channel < lut.common.input_channels; channel++) {
        TRY(icc_lut8_map_1d(
            lut.input_table,
            input.channels[channel],
            channel,
            &temp[channel]
        ));
    }

    TRY(icc_lut8_clut_map(lut, temp, out));

    for (size_t channel = 0; channel < lut.common.output_channels; channel++) {
        TRY(icc_lut8_map_1d(
            lut.output_table,
            out[channel],
            channel,
            &out[channel]
        ));
    }

    return NULL;
}

Error* icc_parse_lut16(ParseCtx ctx, IccLut16* out) {
    RELEASE_ASSERT(ctx.buffer);
    RELEASE_ASSERT(out);

    TRY(icc_parse_lut_common(&ctx, &out->common));
    if (out->common.signature != 0x6D667432) {
        return ERROR(ICC_ERR_INVALID_SIGNATURE);
    }

    TRY(parse_ctx_read_u16_be(&ctx, &out->input_entries));
    TRY(parse_ctx_read_u16_be(&ctx, &out->output_entries));

    TRY(parse_ctx_new_subctx(
        &ctx,
        ctx.offset,
        2 * (size_t)out->input_entries * (size_t)out->common.input_channels,
        &out->input_table
    ));

    TRY(parse_ctx_new_subctx(
        &ctx,
        ctx.offset,
        2 * integer_pow(out->common.grid_points, out->common.input_channels)
            * (size_t)out->common.output_channels,
        &out->clut
    ));

    TRY(parse_ctx_new_subctx(
        &ctx,
        ctx.offset,
        2 * (size_t)out->output_entries * (size_t)out->common.output_channels,
        &out->output_table
    ));

    return NULL;
}

static Error* icc_lut16_map_1d(
    ParseCtx table,
    double val,
    size_t channel,
    size_t entries,
    double* out
) {
    LOG_DIAG(TRACE, ICC, "Mapping 1d 16bit: %f or %f", val, val * 65535.0);

    val = val < 0.0 ? 0.0 : val;
    val = val > 1.0 ? 1.0 : val;

    double x = val * (double)(entries - 1) + (double)channel * (double)entries;
    double frac = x - floor(x);
    bool interpolate =
        (size_t)floor(x) < entries + (size_t)channel * entries - 1;

    uint16_t sample_a;
    TRY(parse_ctx_seek(&table, 2 * (size_t)floor(x)));
    TRY(parse_ctx_read_u16_be(&table, &sample_a));

    if (interpolate) {
        uint16_t sample_b;
        TRY(parse_ctx_read_u16_be(&table, &sample_b));

        double a = (double)sample_a / 65535.0;
        double b = (double)sample_b / 65535.0;
        *out = a + frac * (b - a);
    } else {
        *out = (double)sample_a / 65535.0;
    }

    LOG_DIAG(TRACE, ICC, "Out: %f or %f", *out, *out * 65535.0);

    return NULL;
}

static Error*
icc_lut16_clut_map(IccLut16 lut, double coords[15], double out[15]) {
    RELEASE_ASSERT(coords);
    RELEASE_ASSERT(out);
    RELEASE_ASSERT(lut.common.input_channels >= 1);

    double max_coord = (double)lut.common.grid_points - 1.0;
    for (size_t idx = 0; idx < lut.common.input_channels; idx++) {
        coords[idx] = coords[idx] < 0.0 ? 0.0 : coords[idx];
        coords[idx] = coords[idx] > 1.0 ? 1.0 : coords[idx];
        coords[idx] *= max_coord;
        LOG_DIAG(
            DEBUG,
            ICC,
            "CLut scaled input coord %zu: %f/%f",
            idx,
            coords[idx],
            max_coord
        );
    }

    size_t strides[15];
    strides[lut.common.input_channels - 1] = 1;
    for (int idx = (int)lut.common.input_channels - 2; idx >= 0; idx--) {
        strides[idx] = strides[idx + 1] * lut.common.grid_points;
    }

    double acc[15] = {0}; // TODO: Kahan summation
    uint16_t max_state = (uint16_t)(1 << lut.common.input_channels) - 1;
    for (uint16_t state = 0; state <= max_state; state++) {
        size_t offset = 0;
        double weight = 1.0;
        for (size_t in_channel = 0; in_channel < lut.common.input_channels;
             in_channel++) {
            bool high = ((state >> in_channel) & 1) == 1;
            double frac = coords[in_channel] - floor(coords[in_channel]);
            weight *= high ? frac : (1.0 - frac);

            double coord = coords[in_channel] + (high ? 1.0 : 0.0);
            coord = coord > max_coord ? max_coord : coord;
            offset += (size_t)coord * strides[in_channel];
        }

        offset *= (size_t)lut.common.output_channels * 2;
        TRY(parse_ctx_seek(&lut.clut, offset));

        for (size_t out_channel = 0; out_channel < lut.common.output_channels;
             out_channel++) {
            uint16_t sample;
            TRY(parse_ctx_read_u16_be(&lut.clut, &sample));
            acc[out_channel] += (double)sample * weight;
        }
    }

    for (size_t output = 0; output < lut.common.output_channels; output++) {
        out[output] = acc[output] / 65535.0;
        LOG_DIAG(DEBUG, ICC, "CLut output coord %zu: %f", output, acc[output]);
    }

    return NULL;
}

Error* icc_lut16_map(IccLut16 lut, IccColor input, double out[15]) {
    RELEASE_ASSERT(out);

    LOG_DIAG(DEBUG, ICC, "Applying lut16");

    if (icc_color_space_channels(input.color_space)
        != lut.common.input_channels) {
        return ERROR(
            ICC_ERR_INCORRECT_CHANNELS,
            "Input space %d has %d channels, but the lut has %d",
            (int)input.color_space,
            (int)icc_color_space_channels(input.color_space),
            (int)lut.common.input_channels
        );
    }

    icc_color_norm_pcs(&input, lut.common.matrix);

    double temp[15] = {0};
    for (size_t channel = 0; channel < lut.common.input_channels; channel++) {
        TRY(icc_lut16_map_1d(
            lut.input_table,
            input.channels[channel],
            channel,
            lut.input_entries,
            &temp[channel]
        ));
    }

    TRY(icc_lut16_clut_map(lut, temp, out));

    for (size_t channel = 0; channel < lut.common.output_channels; channel++) {
        TRY(icc_lut16_map_1d(
            lut.output_table,
            out[channel],
            channel,
            lut.output_entries,
            &out[channel]
        ));
    }

    return NULL;
}

static Error* icc_parse_variable_clut(
    ParseCtx* ctx,
    uint8_t output_channels,
    IccVariableCLut* out
) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);

    TRY(parse_ctx_bound_check(ctx, 20));

    size_t len = output_channels;
    for (size_t dim = 0; dim < 16; dim++) {
        TRY(parse_ctx_read_u8(ctx, &out->grid_points[dim]));
        if (dim < 3) {
            len *= (size_t)out->grid_points[dim];
        }
    }

    TRY(parse_ctx_read_u8(ctx, &out->precision));
    TRY(parse_ctx_read_u16_be(ctx, &out->padding));
    TRY(parse_ctx_read_u8(ctx, &out->padding2));

    if (out->precision != 1 && out->precision != 2) {
        return ERROR(
            ICC_ERR_INVALID_LUT,
            "CLut precision must be 1 or 2 bytes"
        );
    }
    len *= out->precision;

    TRY(parse_ctx_new_subctx(ctx, ctx->offset, len, &out->data));
    return NULL;
}

static Error* icc_variable_clut_map(
    IccVariableCLut lut,
    uint8_t input_channels,
    uint8_t output_channels,
    double coords[15],
    double out[15]
) {
    RELEASE_ASSERT(coords);
    RELEASE_ASSERT(out);
    RELEASE_ASSERT(input_channels >= 1);

    for (size_t idx = 0; idx < input_channels; idx++) {
        coords[idx] = coords[idx] < 0.0 ? 0.0 : coords[idx];
        coords[idx] = coords[idx] > 1.0 ? 1.0 : coords[idx];
        coords[idx] *= (double)lut.grid_points[idx] - 1.0;
        LOG_DIAG(
            DEBUG,
            ICC,
            "CLut scaled input coord %zu: %f/%f",
            idx,
            coords[idx],
            (double)lut.grid_points[idx] - 1.0
        );
    }

    size_t strides[15];
    strides[input_channels - 1] = 1;
    for (int idx = (int)input_channels - 2; idx >= 0; idx--) {
        strides[idx] = strides[idx + 1] * lut.grid_points[idx];
    }

    double acc[15] = {0}; // TODO: Kahan summation
    uint16_t max_state = (uint16_t)(1 << input_channels) - 1;

    for (uint16_t state = 0; state <= max_state; state++) {
        size_t offset = 0;
        double weight = 1.0;
        for (size_t in_channel = 0; in_channel < input_channels; in_channel++) {
            double max_coord = (double)lut.grid_points[in_channel] - 1.0;

            bool high = ((state >> in_channel) & 1) == 1;
            double frac = coords[in_channel] - floor(coords[in_channel]);
            weight *= high ? frac : (1.0 - frac);

            double coord = coords[in_channel] + (high ? 1.0 : 0.0);
            coord = coord > max_coord ? max_coord : coord;
            offset += (size_t)coord * strides[in_channel];
        }

        offset *= (size_t)output_channels * lut.precision;
        TRY(parse_ctx_seek(&lut.data, offset));

        for (size_t out_channel = 0; out_channel < output_channels;
             out_channel++) {
            if (lut.precision == 1) {
                uint8_t sample;
                TRY(parse_ctx_read_u8(&lut.data, &sample));
                acc[out_channel] += (double)sample * weight;
            } else {
                uint16_t sample;
                TRY(parse_ctx_read_u16_be(&lut.data, &sample));
                acc[out_channel] += (double)sample * weight;
            }
        }
    }

    for (size_t output = 0; output < output_channels; output++) {
        out[output] = acc[output] / (lut.precision == 1 ? 255.0 : 65535.0);
        LOG_DIAG(DEBUG, ICC, "CLut output coord %zu: %f", output, acc[output]);
    }

    return NULL;
}

Error* icc_parse_lut_b_to_a(Arena* arena, ParseCtx ctx, IccLutBToA* out) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(out);

    LOG_DIAG(DEBUG, ICC, "Applying b->a lut");

    TRY(parse_ctx_seek(&ctx, 0));
    TRY(parse_ctx_read_u32_be(&ctx, &out->signature));
    TRY(parse_ctx_read_u32_be(&ctx, &out->reserved));
    TRY(parse_ctx_read_u8(&ctx, &out->input_channels));
    TRY(parse_ctx_read_u8(&ctx, &out->output_channels));
    TRY(parse_ctx_read_u16_be(&ctx, &out->padding));
    TRY(parse_ctx_read_u32_be(&ctx, &out->b_curves_offset));
    TRY(parse_ctx_read_u32_be(&ctx, &out->matrix_offset));
    TRY(parse_ctx_read_u32_be(&ctx, &out->m_curves_offset));
    TRY(parse_ctx_read_u32_be(&ctx, &out->clut_offset));
    TRY(parse_ctx_read_u32_be(&ctx, &out->a_curves_offset));

    if (out->signature != 0x6D424120) {
        return ERROR(ICC_ERR_INVALID_SIGNATURE);
    }
    RELEASE_ASSERT(out->input_channels == 3);

    if (out->reserved != 0 || out->padding != 0) {
        LOG_WARN(ICC, "Reserved field not zero");
    }

    TRY(parse_ctx_seek(&ctx, out->b_curves_offset));
    for (uint8_t idx = 0; idx < out->input_channels; idx++) {
        TRY(parse_ctx_align(&ctx, 4, true));
        TRY(icc_parse_any_curve(&ctx, &out->b_curves[idx]));
    }

    if (out->matrix_offset != 0) {
        if (out->output_channels != 3
            && (out->m_curves_offset == 0 || out->input_channels != 3)) {
            return ERROR(
                ICC_ERR_INVALID_LUT,
                "Matrix only allowed on BtoA lut if the number of output channels or m curves is 3"
            );
        }

        out->has_matrix = true;
        TRY(parse_ctx_seek(&ctx, out->matrix_offset));
        TRY(parse_ctx_align(&ctx, 4, true));

        double e[12] = {0};
        for (size_t idx = 0; idx < 12; idx++) {
            IccS15Fixed16Number element;
            TRY(parse_ctx_read_i32_be(&ctx, &element));
            e[idx] = icc_s15_fixed16_to_double(element);
        }

        out->matrix =
            geom_mat3_new(e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7], e[8]);
        out->matrix_vec = geom_vec3_new(e[9], e[10], e[11]);
    } else {
        out->has_matrix = false;
    }

    if (out->m_curves_offset != 0) {
        if (!out->has_matrix) {
            return ERROR(
                ICC_ERR_INVALID_LUT,
                "M-curves can only be used if the matrix is used"
            );
        }

        TRY(parse_ctx_seek(&ctx, out->m_curves_offset));
        for (size_t idx = 0; idx < out->input_channels; idx++) {
            TRY(parse_ctx_align(&ctx, 4, true));

            TRY(icc_parse_any_curve(&ctx, &out->m_curves[idx]));
        }
    } else {
        if (out->has_matrix) {
            return ERROR(
                ICC_ERR_INVALID_LUT,
                "M-curves must be used if matrix is used"
            );
        }
    }

    if (out->clut_offset != 0) {
        out->has_clut = true;
        TRY(parse_ctx_seek(&ctx, out->clut_offset));
        TRY(parse_ctx_align(&ctx, 4, true));
        TRY(icc_parse_variable_clut(&ctx, out->output_channels, &out->clut));
    } else {
        out->has_clut = false;
    }

    if (out->a_curves_offset != 0) {
        if (!out->has_clut) {
            return ERROR(
                ICC_ERR_INVALID_LUT,
                "A-curves can only be used if the clut is used"
            );
        }

        TRY(parse_ctx_seek(&ctx, out->a_curves_offset));
        out->a_curves = icc_any_curve_vec_new(arena);
        for (size_t idx = 0; idx < out->output_channels; idx++) {
            TRY(parse_ctx_align(&ctx, 4, true));

            IccAnyCurve curve;
            TRY(icc_parse_any_curve(&ctx, &curve));
            icc_any_curve_vec_push(out->a_curves, curve);
        }
    } else {
        if (out->has_clut) {
            return ERROR(
                ICC_ERR_INVALID_LUT,
                "A-curves must be used if clut is used"
            );
        }

        out->a_curves = NULL;
    }

    return NULL;
}

Error*
icc_lut_b_to_a_map(const IccLutBToA* lut, IccPcsColor input, double out[15]) {
    RELEASE_ASSERT(lut);
    RELEASE_ASSERT(out);

    IccPcsColor intermediate = input;
    TRY(icc_any_curve_map(
        lut->b_curves[0],
        input.is_xyz,
        input.vec.x,
        &intermediate.vec.x
    ));
    TRY(icc_any_curve_map(
        lut->b_curves[1],
        input.is_xyz,
        input.vec.y,
        &intermediate.vec.y
    ));
    TRY(icc_any_curve_map(
        lut->b_curves[2],
        input.is_xyz,
        input.vec.z,
        &intermediate.vec.z
    ));

    if (lut->has_matrix) {
        LOG_DIAG(TRACE, ICC, "Applied matrix and offset");
        intermediate.vec = geom_vec3_add(
            geom_vec3_transform(intermediate.vec, lut->matrix),
            lut->matrix_vec
        );

        TRY(icc_any_curve_map(
            lut->m_curves[0],
            intermediate.is_xyz,
            intermediate.vec.x,
            &intermediate.vec.x
        ));
        TRY(icc_any_curve_map(
            lut->m_curves[1],
            intermediate.is_xyz,
            intermediate.vec.y,
            &intermediate.vec.y
        ));
        TRY(icc_any_curve_map(
            lut->m_curves[2],
            intermediate.is_xyz,
            intermediate.vec.z,
            &intermediate.vec.z
        ));
    }

    if (lut->has_clut) {
        double coords[15] =
            {intermediate.vec.x, intermediate.vec.y, intermediate.vec.z};
        TRY(icc_variable_clut_map(
            lut->clut,
            lut->input_channels,
            lut->output_channels,
            coords,
            out
        ));

        for (size_t idx = 0; idx < icc_any_curve_vec_len(lut->a_curves);
             idx++) {
            IccAnyCurve a_curve;
            RELEASE_ASSERT(icc_any_curve_vec_get(lut->a_curves, idx, &a_curve));
            TRY(icc_any_curve_map(a_curve, false, out[idx], &out[idx]));
        }
    } else {
        out[0] = intermediate.vec.x;
        out[1] = intermediate.vec.y;
        out[2] = intermediate.vec.z;
    }

    return NULL;
}
