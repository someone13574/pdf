#include "font.h"

#include <string.h>

#include "logger/log.h"
#include "pdf/fonts/font.h"

uint32_t
next_cid(PdfFont* font, PdfString* data, size_t* offset, bool* finished) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(data);
    RELEASE_ASSERT(offset);
    RELEASE_ASSERT(finished);

    switch (font->type) {
        case PDF_FONT_TYPE0: {
            // Length check is done by deserializer, so assert is valid
            void* descendant_font_ptr;
            RELEASE_ASSERT(pdf_void_vec_get(
                font->data.type0.descendant_fonts.elements,
                0,
                &descendant_font_ptr
            ));

            PdfCIDFont* cid_font = descendant_font_ptr;
            PdfFont descendant_font = (PdfFont
            ) {.type = (strcmp(cid_font->subtype, "CIDFontType0") == 0)
                         ? PDF_FONT_CIDTYPE0
                         : PDF_FONT_CIDTYPE2,
               .data.cid = *cid_font};

            return next_cid(&descendant_font, data, offset, finished);
        }
        case PDF_FONT_CIDTYPE0: {
            *offset += 1;
            if (*offset == data->len) {
                *finished = true;
            }
            return 0;
        }
        default: {
            LOG_TODO("CID decoding for font type %d", font->type);
        }
    }

    LOG_PANIC("Unreachable");
}
