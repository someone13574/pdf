#include "font.h"

#include <string.h>

#include "logger/log.h"
#include "pdf/fonts/cmap.h"
#include "pdf/fonts/font.h"
#include "pdf_error/error.h"

PdfError* next_cid(
    PdfFont* font,
    PdfCMapCache* cmap_cache,
    PdfString* data,
    size_t* offset,
    bool* finished_out,
    uint32_t* cid_out
) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(cmap_cache);
    RELEASE_ASSERT(data);
    RELEASE_ASSERT(offset);
    RELEASE_ASSERT(finished_out);
    RELEASE_ASSERT(cid_out);

    switch (font->type) {
        case PDF_FONT_TYPE0: {
            if (*offset + 1 >= data->len) {
                *finished_out = true;
                return NULL;
            }

            // Get CMap. TODO: Check ROS against descendent font's ROS
            PdfCMap* cmap = NULL;
            PDF_PROPAGATE(
                pdf_cmap_cache_get(cmap_cache, font->data.type0.encoding, &cmap)
            );

            // Get codepoint
            RELEASE_ASSERT(*offset + 1 < data->len);
            uint32_t codepoint = ((uint32_t)data->data[*offset] << 8)
                               | (uint32_t)data->data[*offset + 1];

            // Map codepoint to cid
            PDF_PROPAGATE(pdf_cmap_get_cid(cmap, codepoint, cid_out));

            *offset += 2;

            return NULL;
        }
        default: {
            LOG_TODO("CID decoding for font type %d", font->type);
        }
    }

    LOG_PANIC("Unreachable");
}
