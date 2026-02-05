#include "font.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena/arena.h"
#include "arena/common.h"
#include "cache.h"
#include "canvas/canvas.h"
#include "cff/cff.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "logger/log.h"
#include "parse_ctx/ctx.h"
#include "pdf/fonts/agl.h"
#include "pdf/fonts/cid_to_gid_map.h"
#include "pdf/fonts/cmap.h"
#include "pdf/fonts/encoding.h"
#include "pdf/fonts/font.h"
#include "pdf/fonts/font_descriptor.h"
#include "pdf/fonts/stream_dict.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/stream_dict.h"
#include "sfnt/glyph.h"
#include "sfnt/sfnt.h"

Error* next_cid(
    PdfFont* font,
    RenderCache* cache,
    PdfString* data,
    size_t* offset,
    bool* finished_out,
    uint32_t* cid_out
) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(cache);
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
            TRY(pdf_cmap_cache_get(
                cache->cmap_cache,
                font->data.type0.encoding,
                &cmap
            ));

            // Get codepoint
            RELEASE_ASSERT(*offset + 1 < data->len);
            uint32_t codepoint = ((uint32_t)data->data[*offset] << 8)
                               | (uint32_t)data->data[*offset + 1];

            // Map codepoint to cid
            TRY(pdf_cmap_get_cid(cmap, codepoint, cid_out));

            *offset += 2;

            return NULL;
        }
        case PDF_FONT_TRUETYPE: {
            if (*offset >= data->len) {
                *finished_out = true;
                return NULL;
            }

            *cid_out = (uint32_t)data->data[*offset];
            *offset += 1;
            return NULL;
        }
        default: {
            LOG_TODO("CID decoding for font type %d", font->type);
        }
    }

    LOG_PANIC("Unreachable");
}

Error* cid_to_gid(
    Arena* arena,
    PdfFont* font,
    RenderCache* cache,
    PdfResolver* resolver,
    uint32_t cid,
    uint32_t* gid_out
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(cache);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(gid_out);

    switch (font->type) {
        case PDF_FONT_TYPE0: {
            // Get descendent font (bound checked by deserialized)
            PdfCIDFont cid_font;
            RELEASE_ASSERT(pdf_cid_font_vec_get(
                font->data.type0.descendant_fonts,
                0,
                &cid_font
            ));

            PdfFont descendent_font = {
                .type =
                    (strcmp(cid_font.subtype, "CIDFontType0") == 0
                         ? PDF_FONT_CIDTYPE0
                         : PDF_FONT_CIDTYPE2),
                .data.cid = cid_font
            };

            // Call recursively
            TRY(cid_to_gid(
                arena,
                &descendent_font,
                cache,
                resolver,
                cid,
                gid_out
            ));
            return NULL;
        }
        case PDF_FONT_CIDTYPE0: {
            // PdfFontDescriptor font_descriptor;
            TRY(pdf_resolve_font_descriptor(
                &font->data.cid.font_descriptor,
                resolver
            ));

            PdfFontDescriptor* font_descriptor =
                font->data.cid.font_descriptor.resolved;

            if (font_descriptor->font_file.is_some) {
                LOG_TODO("Embedded Type1 font");
            } else if (font_descriptor->font_file2.is_some) {
                LOG_TODO("Embedded Type2 font");
            } else if (font_descriptor->font_file3.is_some) {
                PdfFontStreamDict stream_dict;
                TRY(pdf_deserde_font_stream_dict(
                    font_descriptor->font_file3.value.stream_dict->raw_dict,
                    &stream_dict,
                    resolver
                ));

                if (!stream_dict.subtype.is_some) {
                    return ERROR(
                        PDF_ERR_MISSING_DICT_KEY,
                        "`Subtype` is required for FontFile3"
                    );
                }

                PdfName subtype = stream_dict.subtype.value;
                if (strcmp(subtype, "Type1C") == 0) {
                    LOG_TODO("Type1C FontFile3 embedded font");
                } else if (strcmp(subtype, "CIDFontType0C") == 0) {
                    *gid_out = cid; // Font is CID keyed
                } else {
                    LOG_TODO("Make this an error");
                }
            } else {
                LOG_TODO("Non-embedded CIDType0 fonts");
            }

            return NULL;
        }
        case PDF_FONT_CIDTYPE2: {
            RELEASE_ASSERT(
                font->data.cid.cid_to_gid_map.is_some,
                "TODO: Non-embedded cid2 fonts"
            );

            TRY(pdf_map_cid_to_gid(
                &font->data.cid.cid_to_gid_map.value,
                cid,
                gid_out
            ));
            break;
        }
        case PDF_FONT_TRUETYPE: {
            RELEASE_ASSERT(font->data.true_type.encoding.is_some);
            RELEASE_ASSERT(cid < 256);

            if (font->data.true_type.to_unicode.is_some) {
                PdfCMap* to_unicode = NULL;
                TRY(pdf_parse_cmap(
                    arena,
                    font->data.true_type.to_unicode.value.stream_bytes,
                    font->data.true_type.to_unicode.value.decoded_stream_len,
                    &to_unicode
                ));

                uint32_t unicode;
                TRY(pdf_cmap_get_unicode(to_unicode, cid, &unicode));
                *gid_out = unicode;
            } else {
                const char* glyph = pdf_encoding_map_codepoint(
                    &font->data.true_type.encoding.value,
                    (uint8_t)cid
                );
                RELEASE_ASSERT(glyph);

                if (!cache->glyph_list) {
                    uint8_t* glyph_list = load_file_to_buffer(
                        arena,
                        "assets/agl-aglfn/glyphlist.txt",
                        NULL
                    );
                    RELEASE_ASSERT(glyph_list);

                    cache->glyph_list =
                        pdf_parse_agl_glyphlist(arena, (char*)glyph_list);
                }

                uint16_t codepoints[4] = {0, 0, 0, 0};
                uint8_t num_codepoints = 0;
                TRY(pdf_agl_glyphlist_lookup(
                    cache->glyph_list,
                    glyph,
                    codepoints,
                    &num_codepoints
                ));

                RELEASE_ASSERT(
                    num_codepoints == 1,
                    ", TODO: Multi-codepoint glyphs"
                );

                *gid_out = codepoints[0];
            }

            break;
        }
        default: {
            LOG_TODO("CID to GID mapping for font type %d", font->type);
        }
    }

    return NULL;
}

Error* render_glyph(
    Arena* arena,
    PdfFont* font,
    PdfResolver* resolver,
    uint32_t gid,
    Canvas* canvas,
    GeomMat3 transform,
    CanvasBrush brush
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(resolver);

    switch (font->type) {
        case PDF_FONT_TYPE0: {
            // Get descendent font (bound checked by deserialized)
            PdfCIDFont cid_font;
            RELEASE_ASSERT(pdf_cid_font_vec_get(
                font->data.type0.descendant_fonts,
                0,
                &cid_font
            ));

            PdfFont descendent_font = {
                .type =
                    (strcmp(cid_font.subtype, "CIDFontType0") == 0
                         ? PDF_FONT_CIDTYPE0
                         : PDF_FONT_CIDTYPE2),
                .data.cid = cid_font
            };

            // Call recursively
            TRY(render_glyph(
                arena,
                &descendent_font,
                resolver,
                gid,
                canvas,
                transform,
                brush
            ));
            return NULL;
        }
        case PDF_FONT_CIDTYPE0: {
            TRY(pdf_resolve_font_descriptor(
                &font->data.cid.font_descriptor,
                resolver
            ));
            PdfFontDescriptor* font_descriptor =
                font->data.cid.font_descriptor.resolved;

            if (font_descriptor->font_file.is_some) {
                LOG_TODO("Embedded Type1 font");
            } else if (font_descriptor->font_file2.is_some) {
                LOG_TODO("Embedded Type2 font");
            } else if (font_descriptor->font_file3.is_some) {
                PdfFontStreamDict stream_dict;
                TRY(pdf_deserde_font_stream_dict(
                    font_descriptor->font_file3.value.stream_dict->raw_dict,
                    &stream_dict,
                    resolver
                ));

                if (!stream_dict.subtype.is_some) {
                    return ERROR(
                        PDF_ERR_MISSING_DICT_KEY,
                        "`Subtype` is required for FontFile3"
                    );
                }

                PdfName subtype = stream_dict.subtype.value;
                if (strcmp(subtype, "Type1C") == 0) {
                    LOG_TODO("Type1C FontFile3 embedded font");
                } else if (strcmp(subtype, "CIDFontType0C") == 0) {
                    Arena* local_arena = arena_new(1024);
                    CffFontSet* cff_font_set;
                    ParseCtx cff_ctx = parse_ctx_new(
                        font_descriptor->font_file3.value.stream_bytes,
                        font_descriptor->font_file3.value.decoded_stream_len
                    );
                    TRY(cff_parse_fontset(arena, cff_ctx, &cff_font_set));

                    TRY(cff_render_glyph(
                        cff_font_set,
                        gid,
                        canvas,
                        transform,
                        brush
                    ));

                    arena_free(local_arena);
                } else {
                    LOG_TODO("Make this an error");
                }
            } else {
                LOG_TODO("Non-embedded CIDType0 fonts");
            }

            return NULL;
        }
        case PDF_FONT_CIDTYPE2: {
            TRY(pdf_resolve_font_descriptor(
                &font->data.cid.font_descriptor,
                resolver
            ));
            PdfFontDescriptor* font_descriptor =
                font->data.cid.font_descriptor.resolved;

            SfntFont sfnt_font;
            if (font_descriptor->font_file2.is_some) {
                TRY(sfnt_font_new(
                    arena,
                    parse_ctx_new(
                        font_descriptor->font_file2.value.stream_bytes,
                        font_descriptor->font_file2.value.decoded_stream_len
                    ),
                    &sfnt_font
                ));
            } else {
                // TODO: proper resolution, caching
                TRY(sfnt_font_new(
                    arena,
                    parse_ctx_from_file(
                        arena,
                        "assets/fonts-urw-base35/fonts/NimbusSans-Regular.ttf"
                    ),
                    &sfnt_font
                ));
            }

            SfntGlyph glyph;
            TRY(sfnt_get_glyph_for_gid(&sfnt_font, gid, &glyph));
            sfnt_glyph_render(canvas, &glyph, transform, brush);

            return NULL;
        }
        case PDF_FONT_TRUETYPE: {
            RELEASE_ASSERT(font->data.true_type.font_descriptor.is_some);

            TRY(pdf_resolve_font_descriptor(
                &font->data.true_type.font_descriptor.value,
                resolver
            ));
            PdfFontDescriptor* font_descriptor =
                font->data.true_type.font_descriptor.value.resolved;

            SfntFont sfnt_font;
            if (font_descriptor->font_file2.is_some) {
                TRY(sfnt_font_new(
                    arena,
                    parse_ctx_new(
                        font_descriptor->font_file2.value.stream_bytes,
                        font_descriptor->font_file2.value.decoded_stream_len
                    ),
                    &sfnt_font
                ));
            } else {
                // TODO: proper resolution, caching
                TRY(sfnt_font_new(
                    arena,
                    parse_ctx_from_file(
                        arena,
                        "assets/fonts-urw-base35/fonts/NimbusSans-Regular.ttf"
                    ),
                    &sfnt_font
                ));
            }

            SfntGlyph glyph;
            TRY(sfnt_get_glyph_for_cid(&sfnt_font, gid, &glyph));
            sfnt_glyph_render(canvas, &glyph, transform, brush);
            break;
        }
        default: {
            LOG_TODO("Glyph rendering for font type %d", font->type);
        }
    }

    return NULL;
}

Error* cid_to_width(
    PdfFont* font,
    PdfResolver* resolver,
    uint32_t cid,
    PdfNumber* width_out
) {
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(width_out);

    switch (font->type) {
        case PDF_FONT_TYPE0: {
            // TODO: Make this process of getting the descendent font a
            // function...
            PdfCIDFont cid_font;
            RELEASE_ASSERT(pdf_cid_font_vec_get(
                font->data.type0.descendant_fonts,
                0,
                &cid_font
            ));

            PdfFont descendent_font = {
                .type =
                    (strcmp(cid_font.subtype, "CIDFontType0") == 0
                         ? PDF_FONT_CIDTYPE0
                         : PDF_FONT_CIDTYPE2),
                .data.cid = cid_font
            };

            // Call recursively
            TRY(cid_to_width(&descendent_font, resolver, cid, width_out));
            return NULL;
            break;
        }
        case PDF_FONT_CIDTYPE0:
        case PDF_FONT_CIDTYPE2: {
            PdfCIDFont* cid_font = &font->data.cid;

            if (cid_font->w.is_some) {
                PdfFontWidthEntry width_entry;
                if (pdf_font_width_vec_get(
                        cid_font->w.value.cid_to_width,
                        cid,
                        &width_entry
                    )
                    && width_entry.has_value) {
                    *width_out =
                        (PdfNumber) {.type = PDF_NUMBER_TYPE_INTEGER,
                                     .value.integer = width_entry.width};
                    return NULL;
                }
            }

            if (cid_font->dw.is_some) {
                *width_out = (PdfNumber) {.type = PDF_NUMBER_TYPE_INTEGER,
                                          .value.integer = cid_font->dw.value};
                return NULL;
            }

            *width_out = (PdfNumber) {.type = PDF_NUMBER_TYPE_INTEGER,
                                      .value.integer = 1000};
            break;
        }
        case PDF_FONT_TRUETYPE: {
            RELEASE_ASSERT(font->data.true_type.widths.is_some);
            RELEASE_ASSERT(font->data.true_type.first_char.is_some);

            if (!pdf_number_vec_get(
                    font->data.true_type.widths.value,
                    cid - (uint32_t)font->data.true_type.first_char.value,
                    width_out
                )) {
                RELEASE_ASSERT(font->data.true_type.font_descriptor.is_some);

                TRY(pdf_resolve_font_descriptor(
                    &font->data.true_type.font_descriptor.value,
                    resolver
                ));
                PdfFontDescriptor* font_descriptor =
                    font->data.true_type.font_descriptor.value.resolved;

                RELEASE_ASSERT(font_descriptor->missing_width.is_some);
                *width_out = font_descriptor->missing_width.value;
            }

            break;
        }
        default: {
            LOG_TODO("CID width for font type %d", font->type);
        }
    }

    return NULL;
}

Error* get_font_matrix(
    Arena* arena,
    PdfResolver* resolver,
    PdfFont* font,
    GeomMat3* font_matrix_out
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(font);
    RELEASE_ASSERT(font_matrix_out);

    double units_per_em = 1000.0;

    switch (font->type) {
        case PDF_FONT_TYPE0: {
            PdfCIDFont cid_font;
            RELEASE_ASSERT(pdf_cid_font_vec_get(
                font->data.type0.descendant_fonts,
                0,
                &cid_font
            ));

            PdfFont descendent_font = {
                .type =
                    (strcmp(cid_font.subtype, "CIDFontType0") == 0
                         ? PDF_FONT_CIDTYPE0
                         : PDF_FONT_CIDTYPE2),
                .data.cid = cid_font
            };

            // Call recursively
            TRY(get_font_matrix(
                arena,
                resolver,
                &descendent_font,
                font_matrix_out
            ));
            return NULL;
        }
        case PDF_FONT_CIDTYPE0: {
            TRY(pdf_resolve_font_descriptor(
                &font->data.cid.font_descriptor,
                resolver
            ));
            PdfFontDescriptor* font_descriptor =
                font->data.cid.font_descriptor.resolved;

            if (font_descriptor->font_file.is_some) {
                LOG_TODO("Embedded Type1 font");
            } else if (font_descriptor->font_file2.is_some) {
                LOG_TODO("Embedded Type2 font");
            } else if (font_descriptor->font_file3.is_some) {
                PdfFontStreamDict stream_dict;
                TRY(pdf_deserde_font_stream_dict(
                    font_descriptor->font_file3.value.stream_dict->raw_dict,
                    &stream_dict,
                    resolver
                ));

                if (!stream_dict.subtype.is_some) {
                    return ERROR(
                        PDF_ERR_MISSING_DICT_KEY,
                        "`Subtype` is required for FontFile3"
                    );
                }

                PdfName subtype = stream_dict.subtype.value;
                if (strcmp(subtype, "Type1C") == 0) {
                    LOG_TODO("Type1C FontFile3 embedded font");
                } else if (strcmp(subtype, "CIDFontType0C") == 0) {
                    Arena* local_arena = arena_new(1024);
                    CffFontSet* cff_font_set;
                    ParseCtx cff_ctx = parse_ctx_new(
                        font_descriptor->font_file3.value.stream_bytes,
                        font_descriptor->font_file3.value.decoded_stream_len
                    );
                    TRY(cff_parse_fontset(arena, cff_ctx, &cff_font_set));

                    *font_matrix_out = cff_font_matrix(cff_font_set);
                    arena_free(local_arena);
                } else {
                    LOG_TODO("Make this an error");
                }
            } else {
                LOG_TODO("Non-embedded CIDType0 fonts");
            }

            return NULL;
        }
        case PDF_FONT_CIDTYPE2: {
            // TODO: proper font resolution, caching, using embedded
            TRY(pdf_resolve_font_descriptor(
                &font->data.cid.font_descriptor,
                resolver
            ));
            PdfFontDescriptor* font_descriptor =
                font->data.cid.font_descriptor.resolved;

            SfntFont sfnt_font;
            if (font_descriptor->font_file2.is_some) {
                TRY(sfnt_font_new(
                    arena,
                    parse_ctx_new(
                        font_descriptor->font_file2.value.stream_bytes,
                        font_descriptor->font_file2.value.decoded_stream_len
                    ),
                    &sfnt_font
                ));
            } else {
                // TODO: proper resolution, caching
                TRY(sfnt_font_new(
                    arena,
                    parse_ctx_from_file(
                        arena,
                        "assets/fonts-urw-base35/fonts/NimbusMonoPS-Regular.ttf"
                    ),
                    &sfnt_font
                ));
            }

            units_per_em = (double)sfnt_font_head(&sfnt_font).units_per_em;
            break;
        }
        case PDF_FONT_TRUETYPE: {
            // TODO: proper font resolution, caching, using embedded
            RELEASE_ASSERT(font->data.true_type.font_descriptor.is_some);

            TRY(pdf_resolve_font_descriptor(
                &font->data.true_type.font_descriptor.value,
                resolver
            ));
            PdfFontDescriptor* font_descriptor =
                font->data.true_type.font_descriptor.value.resolved;

            SfntFont sfnt_font;
            if (font_descriptor->font_file2.is_some) {
                TRY(sfnt_font_new(
                    arena,
                    parse_ctx_new(
                        font_descriptor->font_file2.value.stream_bytes,
                        font_descriptor->font_file2.value.decoded_stream_len
                    ),
                    &sfnt_font
                ));
            } else {
                // TODO: proper resolution, caching
                TRY(sfnt_font_new(
                    arena,
                    parse_ctx_from_file(
                        arena,
                        "assets/fonts-urw-base35/fonts/NimbusMonoPS-Regular.ttf"
                    ),
                    &sfnt_font
                ));
            }

            units_per_em = (double)sfnt_font_head(&sfnt_font).units_per_em;

            break;
        }
        default: {
            LOG_TODO("Font matrix for font type %d", font->type);
        }
    }

    *font_matrix_out = geom_mat3_new(
        1.0 / units_per_em,
        0.0,
        0.0,
        0.0,
        1.0 / units_per_em,
        0.0,
        0.0,
        0.0,
        1.0
    );
    return NULL;
}
