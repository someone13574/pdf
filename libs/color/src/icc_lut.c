#include "color/icc_lut.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "color/icc.h"
#include "color/icc_color.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "logger/log.h"
#include "parse_ctx/ctx.h"

static uint64_t integer_pow(uint64_t base, uint64_t exponent) {
    uint64_t acc = 1;
    for (uint64_t idx = 0; idx < exponent; idx++) {
        acc *= base;
    }

    return acc;
}

static Error* icc_parse_lut_common(ParseCtx* ctx, ICCLutCommon* out) {
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
    TRY(parse_ctx_read_u32_be(ctx, &out->e1));
    TRY(parse_ctx_read_u32_be(ctx, &out->e2));
    TRY(parse_ctx_read_u32_be(ctx, &out->e3));
    TRY(parse_ctx_read_u32_be(ctx, &out->e4));
    TRY(parse_ctx_read_u32_be(ctx, &out->e5));
    TRY(parse_ctx_read_u32_be(ctx, &out->e6));
    TRY(parse_ctx_read_u32_be(ctx, &out->e7));
    TRY(parse_ctx_read_u32_be(ctx, &out->e8));
    TRY(parse_ctx_read_u32_be(ctx, &out->e9));

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

Error* icc_parse_lut8(ParseCtx ctx, ICCLut8* out) {
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
        &out->input_tables
    ));

    TRY(parse_ctx_new_subctx(
        &ctx,
        ctx.offset,
        integer_pow(out->common.grid_points, out->common.input_channels)
            * (size_t)out->common.output_channels,
        &out->clut_values
    ));

    TRY(parse_ctx_new_subctx(
        &ctx,
        ctx.offset,
        256 * (size_t)out->common.output_channels,
        &out->output_tables
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

static Error* icc_lut8_clut(ICCLut8* lut, double coords[15], double out[15]) {
    double max_coord = (double)lut->common.grid_points - 1.0;
    for (size_t idx = 0; idx < lut->common.input_channels; idx++) {
        coords[idx] = coords[idx] < 0.0 ? 0.0 : coords[idx];
        coords[idx] = coords[idx] > 1.0 ? 1.0 : coords[idx];
        coords[idx] *= max_coord;
    }

    size_t strides[15];
    strides[lut->common.input_channels - 1] = 1;
    for (int i = (int)lut->common.input_channels - 2; i >= 0; i--) {
        strides[i] = strides[i + 1] * lut->common.grid_points;
    }

    double acc[15] = {0}; // TODO: Kahan summation
    uint16_t max_state = (uint16_t)(1 << lut->common.input_channels) - 1;
    for (uint16_t state = 0; state <= max_state; state++) {
        size_t offset = 0;
        double weight = 1.0;
        for (size_t in_channel = 0; in_channel < lut->common.input_channels;
             in_channel++) {
            bool high = ((state >> in_channel) & 1) == 1;
            double frac = coords[in_channel] - floor(coords[in_channel]);
            weight *= high ? frac : (1.0 - frac);

            double coord = coords[in_channel] + (high ? 1.0 : 0.0);
            coord = coord > max_coord ? max_coord : coord;
            offset += (size_t)coord * strides[in_channel];
        }

        offset *= (size_t)lut->common.output_channels;
        TRY(parse_ctx_seek(&lut->clut_values, offset));

        for (size_t out_channel = 0; out_channel < lut->common.output_channels;
             out_channel++) {
            uint8_t sample;
            TRY(parse_ctx_read_u8(&lut->clut_values, &sample));
            acc[out_channel] += (double)sample / 255.0 * weight;
        }
    }

    for (size_t output = 0; output < lut->common.output_channels; output++) {
        out[output] = acc[output];
    }

    return NULL;
}

Error* icc_lut8_map(
    ICCLut8* lut,
    ICCColor input,
    double out[15],
    size_t* output_channels
) {
    RELEASE_ASSERT(lut);
    RELEASE_ASSERT(output_channels);
    *output_channels = lut->common.output_channels;

    if (input.color_space != lut->common.input_channels) {
        return ERROR(
            ICC_ERR_INCORRECT_CHANNELS,
            "Input space %d has %d channels, but the lut has %d",
            (int)input.color_space,
            (int)icc_color_space_channels(input.color_space),
            (int)lut->common.input_channels
        );
    }

    // Applies matrix if the color space is pcsxyz. Clamps all channels to 0-1
    icc_color_norm_pcs(&input, lut->matrix);

    double temp[15] = {0};
    for (size_t channel = 0; channel < lut->common.input_channels; channel++) {
        TRY(icc_lut8_map_1d(
            lut->input_tables,
            input.channels[channel],
            channel,
            &temp[channel]
        ));
    }

    TRY(icc_lut8_clut(lut, temp, out));

    for (size_t channel = 0; channel < lut->common.output_channels; channel++) {
        TRY(icc_lut8_map_1d(
            lut->output_tables,
            out[channel],
            channel,
            &out[channel]
        ));
    }

    return NULL;
}
