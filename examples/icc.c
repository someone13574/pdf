#include "color/icc.h"

#include <stdbool.h>
#include <stdint.h>

#include "arena/arena.h"
#include "arena/common.h"
#include "color/icc_color.h"
#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/ctx.h"

int main(void) {
    Arena* arena = arena_new(128);

    size_t swop_buffer_len;
    const uint8_t* swop_buffer = load_file_to_buffer(
        arena,
        "test-files/USWebCoatedSWOP.icc",
        &swop_buffer_len
    );

    size_t srgb_buffer_len;
    const uint8_t* srgb_buffer = load_file_to_buffer(
        arena,
        "assets/icc-profiles/sRGB_v4_ICC_preference.icc",
        &srgb_buffer_len
    );

    ParseCtx swop_ctx = parse_ctx_new(swop_buffer, swop_buffer_len);
    IccProfile swop_profile;
    REQUIRE(icc_parse_profile(swop_ctx, &swop_profile));

    ParseCtx srgb_ctx = parse_ctx_new(srgb_buffer, srgb_buffer_len);
    IccProfile srgb_profile;
    REQUIRE(icc_parse_profile(srgb_ctx, &srgb_profile));

    IccColor input = {
        .channels = {0.4, 0.5, 0.6, 0.2},
        .color_space = ICC_COLOR_SPACE_CMYK
    };

    LOG_DIAG(
        INFO,
        EXAMPLE,
        "Naive conversion: %f, %f, %f",
        (1.0 - input.channels[0]) * (1.0 - input.channels[3]),
        (1.0 - input.channels[1]) * (1.0 - input.channels[3]),
        (1.0 - input.channels[2]) * (1.0 - input.channels[3])
    );

    IccRenderingIntent intent = ICC_INTENT_PERCEPTUAL;

    IccPcsColor src_pcs, dst_pcs;
    REQUIRE(icc_device_to_pcs(swop_profile, intent, input, &src_pcs));
    REQUIRE(icc_pcs_to_pcs(
        swop_profile,
        srgb_profile,
        false,
        intent,
        src_pcs,
        &dst_pcs
    ));

    IccColor mapped_color;
    REQUIRE(
        icc_pcs_to_device(arena, srgb_profile, intent, dst_pcs, &mapped_color)
    );
    RELEASE_ASSERT(mapped_color.color_space == ICC_COLOR_SPACE_RGB);

    LOG_DIAG(
        INFO,
        EXAMPLE,
        "Output sRGB: r=%f, g=%f, b=%f",
        mapped_color.channels[0],
        mapped_color.channels[1],
        mapped_color.channels[2]
    );

    arena_free(arena);
    return 0;
}
