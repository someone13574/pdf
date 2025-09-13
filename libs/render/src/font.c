#include "font.h"

#include <stdio.h>
#include <string.h>

#include "arena/arena.h"
#include "cff/cff.h"
#include "logger/log.h"
#include "pdf/fonts/cmap.h"
#include "pdf/fonts/font.h"
#include "pdf/fonts/font_descriptor.h"
#include "pdf/resolver.h"
#include "pdf/stream/stream_dict.h"
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

PdfError* cid_to_gid(
    PdfFont* font,
    PdfOptionalResolver resolver,
    uint32_t cid,
    uint32_t* gid_out
) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(resolver.present);
    RELEASE_ASSERT(resolver.resolver);
    RELEASE_ASSERT(gid_out);

    switch (font->type) {
        case PDF_FONT_TYPE0: {
            // Get descendent font (bound checked by deserialized)
            void* descendent_font_void_ptr = NULL;
            RELEASE_ASSERT(pdf_void_vec_get(
                font->data.type0.descendant_fonts.elements,
                0,
                &descendent_font_void_ptr
            ));

            PdfCIDFont* cid_font = descendent_font_void_ptr;
            PdfFont descendent_font = {
                .type =
                    (strcmp(cid_font->subtype, "CIDFontType0") == 0
                         ? PDF_FONT_CIDTYPE0
                         : PDF_FONT_CIDTYPE2),
                .data.cid = *cid_font
            };

            // Call recursively
            PDF_PROPAGATE(cid_to_gid(&descendent_font, resolver, cid, gid_out));
            return NULL;
        }
        case PDF_FONT_CIDTYPE0: {
            PdfFontDescriptor font_descriptor;
            PDF_PROPAGATE(pdf_resolve_font_descriptor(
                &font->data.cid.font_descriptor,
                resolver.resolver,
                &font_descriptor
            ));

            if (font_descriptor.font_file.discriminant) {
                LOG_TODO("Embedded Type1 font");
            } else if (font_descriptor.font_file2.discriminant) {
                LOG_TODO("Embedded Type2 font");
            } else if (font_descriptor.font_file3.discriminant) {
                if (!font_descriptor.font_file3.value.stream_dict->subtype
                         .discriminant) {
                    return PDF_ERROR(
                        PDF_ERR_MISSING_DICT_KEY,
                        "Subtype field in stream-dict is required if referenced from FontFile3 in a FontDescriptor"
                    );
                }

                PdfName subtype =
                    font_descriptor.font_file3.value.stream_dict->subtype.value;
                if (strcmp(subtype, "Type1C") == 0) {
                    LOG_TODO("Type1C FontFile3 embedded font");
                } else if (strcmp(subtype, "CIDFontType0C") == 0) {
                    Arena* arena = arena_new(1024);
                    CffFont* cff_font;
                    PDF_PROPAGATE(cff_parse(
                        arena,
                        font_descriptor.font_file3.value.stream_bytes,
                        font_descriptor.font_file3.value.decoded_stream_len,
                        &cff_font
                    ));
                    arena_free(arena);

                    LOG_TODO("CIDFontType0C FontFile3 embedded font");
                } else {
                    LOG_TODO("Make this an error");
                }
            } else {
                LOG_TODO("Non-embedded CIDType0 fonts");
            }

            LOG_TODO();
            return NULL;
        }
        default: {
            LOG_TODO("CID to GID mapping for font type %d", font->type);
        }
    }

    return NULL;
}
