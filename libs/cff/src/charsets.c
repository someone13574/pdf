#include "charsets.h"

#include "arena/arena.h"
#include "logger/log.h"
#include "parser.h"
#include "pdf_error/error.h"
#include "types.h"

#define DARRAY_NAME CffSIDArray
#define DARRAY_LOWERCASE_NAME cff_sid_array
#define DARRAY_TYPE CffSID
#include "arena/darray_impl.h"

PdfError* cff_parse_charset(
    CffParser* parser,
    Arena* arena,
    CffCard16 num_glyphs,
    CffCharset* charset_out
) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(charset_out);

    if (num_glyphs == 0) {
        return PDF_ERROR(
            PDF_ERR_CFF_INVALID_CHARSET,
            "TODO: Check for num_glyphs == 0 elsewhere and error there instead or check for no glyphs and skip everything"
        );
    }

    // There is one less element in the glyph name array than nGlyphs because
    // the .notdef glyph name is omitted.
    charset_out->glyph_names = cff_sid_array_new_init(arena, num_glyphs - 1, 0);

    CffCard8 format;
    PDF_PROPAGATE(cff_parser_read_card8(parser, &format));

    switch (format) {
        case 0: {
            for (CffCard16 idx = 0; idx < num_glyphs - 1; idx++) {
                CffSID sid;
                PDF_PROPAGATE(cff_parser_read_sid(parser, &sid));
                cff_sid_array_set(charset_out->glyph_names, idx, sid);
            }
            break;
        }
        case 1: {
            CffCard16 glyph_idx = 0;
            while (glyph_idx < num_glyphs - 1
            ) { // Check against `- 1` since the first glyph isn't included in
                // all formats, despite only being documented for format0.
                CffSID sid;
                CffCard8 n_left; // doesn't include first, so there are `n_left
                                 // + 1` glyphs covered
                PDF_PROPAGATE(cff_parser_read_sid(parser, &sid));
                PDF_PROPAGATE(cff_parser_read_card8(parser, &n_left));

                for (size_t idx = 0; idx <= (size_t)n_left; idx++) {
                    if (glyph_idx == num_glyphs - 1) {
                        return PDF_ERROR(
                            PDF_ERR_CFF_INVALID_CHARSET,
                            "Charset covers more glyphs than exist"
                        );
                    } else if (sid == 65000) {
                        return PDF_ERROR(PDF_ERR_CFF_INVALID_SID);
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
            CffCard16 glyph_idx = 0;
            while (glyph_idx < num_glyphs - 1
            ) { // Check against `- 1` since the first glyph isn't included in
                // all formats, despite only being documented for format0.
                CffSID sid;
                CffCard16 n_left; // doesn't include first, so there are `n_left
                                  // + 1` glyphs covered
                PDF_PROPAGATE(cff_parser_read_sid(parser, &sid));
                PDF_PROPAGATE(cff_parser_read_card16(parser, &n_left));

                for (size_t idx = 0; idx <= (size_t)n_left; idx++) {
                    if (glyph_idx == num_glyphs - 1) {
                        return PDF_ERROR(
                            PDF_ERR_CFF_INVALID_CHARSET,
                            "Charset covers more glyphs than exist"
                        );
                    } else if (sid == 65000) {
                        return PDF_ERROR(PDF_ERR_CFF_INVALID_SID);
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
            return PDF_ERROR(
                PDF_ERR_CFF_INVALID_CHARSET,
                "Invalid charset format %d",
                (int)format
            );
        }
    }

    return NULL;
}
