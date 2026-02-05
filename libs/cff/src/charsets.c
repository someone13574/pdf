#include "charsets.h"

#include "arena/arena.h"
#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/binary.h"
#include "types.h"

Error* cff_parse_charset(
    ParseCtx* ctx,
    Arena* arena,
    uint16_t num_glyphs,
    CffCharset* charset_out
) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(charset_out);

    if (num_glyphs == 0) {
        return ERROR(
            CFF_ERR_INVALID_CHARSET,
            "TODO: Check for num_glyphs == 0 elsewhere and error there instead or check for no glyphs and skip everything"
        );
    }

    // There is one less element in the glyph name array than nGlyphs because
    // the .notdef glyph name is omitted.
    charset_out->glyph_names = cff_sid_array_new_init(arena, num_glyphs - 1, 0);

    uint8_t format;
    TRY(parse_ctx_read_u8(ctx, &format));

    switch (format) {
        case 0: {
            for (uint16_t idx = 0; idx < num_glyphs - 1; idx++) {
                CffSID sid;
                TRY(cff_read_sid(ctx, &sid));
                cff_sid_array_set(charset_out->glyph_names, idx, sid);
            }
            break;
        }
        case 1: {
            uint16_t glyph_idx = 0;
            while (
                glyph_idx < num_glyphs - 1
            ) { // Check against `- 1` since the first glyph isn't included in
                // all formats, despite only being documented for format0.
                CffSID sid;
                uint8_t n_left; // doesn't include first, so there are `n_left
                                // + 1` glyphs covered
                TRY(cff_read_sid(ctx, &sid));
                TRY(parse_ctx_read_u8(ctx, &n_left));

                for (size_t idx = 0; idx <= (size_t)n_left; idx++) {
                    if (glyph_idx == num_glyphs - 1) {
                        return ERROR(
                            CFF_ERR_INVALID_CHARSET,
                            "Charset covers more glyphs than exist"
                        );
                    } else if (sid == 65000) {
                        return ERROR(CFF_ERR_INVALID_SID);
                    }

                    cff_sid_array_set(
                        charset_out->glyph_names,
                        glyph_idx++,
                        sid++
                    );
                }
            }
            break;
        }
        case 2: {
            uint16_t glyph_idx = 0;
            while (
                glyph_idx < num_glyphs - 1
            ) { // Check against `- 1` since the first glyph isn't included in
                // all formats, despite only being documented for format0.
                CffSID sid;
                uint16_t n_left; // doesn't include first, so there are `n_left
                                 // + 1` glyphs covered
                TRY(cff_read_sid(ctx, &sid));
                TRY(parse_ctx_read_u16_be(ctx, &n_left));

                for (size_t idx = 0; idx <= (size_t)n_left; idx++) {
                    if (glyph_idx == num_glyphs - 1) {
                        return ERROR(
                            CFF_ERR_INVALID_CHARSET,
                            "Charset covers more glyphs than exist"
                        );
                    } else if (sid == 65000) {
                        return ERROR(CFF_ERR_INVALID_SID);
                    }

                    cff_sid_array_set(
                        charset_out->glyph_names,
                        glyph_idx++,
                        sid++
                    );
                }
            }
            break;
        }
        default: {
            return ERROR(
                CFF_ERR_INVALID_CHARSET,
                "Invalid charset format %d",
                (int)format
            );
        }
    }

    return NULL;
}
