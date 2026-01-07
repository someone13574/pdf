#include "color/icc_tags.h"

#include "logger/log.h"

uint32_t icc_tag_signature(IccTag tag) {
    switch (tag) {
        case ICC_TAG_A_TO_B0: {
            return 0x41324230;
        }
        case ICC_TAG_A_TO_B1: {
            return 0x41324231;
        }
        case ICC_TAG_A_TO_B2: {
            return 0x41324232;
        }
        case ICC_TAG_BLUE_MATRIX_COL: {
            return 0x6258595A;
        }
        case ICC_TAG_BLUE_TRC: {
            return 0x62545243;
        }
        case ICC_TAG_B_TO_A0: {
            return 0x42324130;
        }
        case ICC_TAG_B_TO_A1: {
            return 0x42324131;
        }
        case ICC_TAG_B_TO_A2: {
            return 0x42324132;
        }
        case ICC_TAG_B_TO_D0: {
            return 0x42324430;
        }
        case ICC_TAG_B_TO_D1: {
            return 0x42324431;
        }
        case ICC_TAG_B_TO_D2: {
            return 0x42324432;
        }
        case ICC_TAG_B_TO_D3: {
            return 0x42324433;
        }
        case ICC_TAG_CAL_DATE: {
            return 0x63616C74;
        }
        case ICC_TAG_CHAR_TARGET: {
            return 0x74617267;
        }
        case ICC_TAG_CHROM_ADAPTATION: {
            return 0x63686164;
        }
        case ICC_TAG_CHROMATICITY: {
            return 0x6368726D;
        }
        case ICC_TAG_CICP: {
            return 0x63696370;
        }
        case ICC_TAG_COLORANT_ORDER: {
            return 0x636C726F;
        }
        case ICC_TAG_COLORANT_TABLE: {
            return 0x636C7274;
        }
        case ICC_TAG_COLORANT_TABLE_OUT: {
            return 0x636C6F74;
        }
        case ICC_TAG_COLORIMETRIC_INTENT_IMG_STATE: {
            return 0x63696973;
        }
        case ICC_TAG_COPYRIGHT: {
            return 0x63707274;
        }
        case ICC_TAG_DEVICE_MFG_DESC: {
            return 0x646D6E64;
        }
        case ICC_TAG_DEVICE_MODEL: {
            return 0x646D6464;
        }
        case ICC_TAG_D_TO_B0: {
            return 0x44324230;
        }
        case ICC_TAG_D_TO_B1: {
            return 0x44324231;
        }
        case ICC_TAG_D_TO_B2: {
            return 0x44324232;
        }
        case ICC_TAG_D_TO_B3: {
            return 0x44324233;
        }
        case ICC_TAG_GAMUT: {
            return 0x67616D74;
        }
        case ICC_TAG_GRAY_TRC: {
            return 0x6B545243;
        }
        case ICC_TAG_GREEN_MATRIX_COL: {
            return 0x6758595A;
        }
        case ICC_TAG_GREEN_TRC: {
            return 0x67545243;
        }
        case ICC_TAG_LUMINANCE: {
            return 0x6C756D69;
        }
        case ICC_TAG_MEASUREMENT: {
            return 0x6D656173;
        }
        case ICC_TAG_METADATA: {
            return 0x6D657461;
        }
        case ICC_TAG_MEDIA_WHITEPOINT: {
            return 0x77747074;
        }
        case ICC_TAG_NAMED_COLOR2: {
            return 0x6E636C32;
        }
        case ICC_TAG_OUTPUT_RESPONSE: {
            return 0x72657370;
        }
        case ICC_TAG_PERCEPTUAL_RENDERING_INTENT_GAMUT: {
            return 0x72696730;
        }
        case ICC_TAG_PREVIEW0: {
            return 0x70726530;
        }
        case ICC_TAG_PREVIEW1: {
            return 0x70726531;
        }
        case ICC_TAG_PREVIEW2: {
            return 0x70726532;
        }
        case ICC_TAG_PROFILE_DESC: {
            return 0x64657363;
        }
        case ICC_TAG_PROFILE_SEQ_DESC: {
            return 0x70736571;
        }
        case ICC_TAG_PROFILE_SEQ_ID: {
            return 0x70736964;
        }
        case ICC_TAG_RED_MATRIX_COL: {
            return 0x7258595A;
        }
        case ICC_TAG_RED_TRC_TAG: {
            return 0x72545243;
        }
        case ICC_TAG_SAT_RENDERING_INTENT_GAMUT: {
            return 0x72696732;
        }
        case ICC_TAG_TECHNOLOGY: {
            return 0x74656368;
        }
        case ICC_TAG_VIEWING_COND_DESC: {
            return 0x76756564;
        }
        case ICC_TAG_VIEWING_CONDS: {
            return 0x76696577;
        }
    }

    LOG_PANIC("Unreachable");
}
