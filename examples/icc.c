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
    ICCProfile swop_profile;
    REQUIRE(icc_parse_profile(swop_ctx, &swop_profile));

    ParseCtx srgb_ctx = parse_ctx_new(srgb_buffer, srgb_buffer_len);
    ICCProfile srgb_profile;
    REQUIRE(icc_parse_profile(srgb_ctx, &srgb_profile));

    ICCColor input = {
        .channels = {1.0, 0.0, 0.0, 1.0},
        .color_space = ICC_COLOR_SPACE_CMYK
    };

    ICCRenderingIntent intent = ICC_INTENT_MEDIA_RELATIVE;

    ICCPcsColor src_pcs, dst_pcs;
    REQUIRE(icc_device_to_pcs(
        swop_profile,
        ICC_INTENT_MEDIA_RELATIVE,
        input,
        &src_pcs
    ));
    REQUIRE(icc_pcs_to_pcs(
        swop_profile,
        srgb_profile,
        false,
        intent,
        src_pcs,
        &dst_pcs
    ));

    ICCColor mapped_color;
    REQUIRE(icc_pcs_to_device(srgb_profile, intent, dst_pcs, &mapped_color));
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
