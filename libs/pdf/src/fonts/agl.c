#include "pdf/fonts/agl.h"

#include <stdint.h>
#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "pdf_error/error.h"

typedef struct {
    const char* name; // This is unterminated!
    size_t name_len;

    uint16_t codepoints[4];
    uint8_t num_codepoints;
} GlyphListEntry;

#define DVEC_NAME GlyphListEntries
#define DVEC_LOWERCASE_NAME glyph_list_entries
#define DVEC_TYPE GlyphListEntry
#include "arena/dvec_impl.h"

struct PdfAglGlyphList {
    GlyphListEntries* entries;
};

PdfAglGlyphList* pdf_parse_agl_glyphlist(Arena* arena, const char* code) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(code);

    PdfAglGlyphList* glyph_list = arena_alloc(arena, sizeof(PdfAglGlyphList));
    glyph_list->entries = glyph_list_entries_new(arena);

    while (*code) {
        if (*code == '#') {
            while (*code && *code != '\n' && *code != '\r') {
                code += 1;
            }
        } else {
            GlyphListEntry entry = {
                .name = code,
                .name_len = 0,
                .codepoints = {0, 0, 0, 0},
                .num_codepoints = 0
            };

            // Measure length
            while (*code && *code != ';') {
                entry.name_len += 1;
                code += 1;
            }

            RELEASE_ASSERT(*code == ';' && entry.name_len > 0);
            code += 1;

            // Parse codepoints
            for (size_t codepoint_idx = 0; codepoint_idx < 4; codepoint_idx++) {
                entry.num_codepoints += 1;

                for (size_t char_idx = 0; char_idx < 4; char_idx++, code++) {
                    if (*code >= '0' && *code <= '9') {
                        entry.codepoints[codepoint_idx] |=
                            (uint16_t)((uint16_t)(*code - '0')
                                       << (uint16_t)(4 * (3 - char_idx)));
                    } else if (*code >= 'A' && *code <= 'F') {
                        entry.codepoints[codepoint_idx] |=
                            (uint16_t)((uint16_t)(*code - 'A' + 10)
                                       << (uint16_t)(4 * (3 - char_idx)));
                    } else {
                        LOG_PANIC("Unreachable: `%c`", *code);
                    }
                }

                if (*code == ' ') {
                    code += 1;
                    continue;
                }
                break;
            }

            while (*code && *code != '\n' && *code != '\r') {
                code += 1;
            }

            glyph_list_entries_push(glyph_list->entries, entry);
        }

        while (*code && (*code == '\n' || *code == '\r')) {
            code += 1;
        }
    }

    return glyph_list;
}

PdfError* pdf_agl_glyphlist_lookup(
    PdfAglGlyphList* glyphlist,
    const char* glyph_name,
    uint16_t* codepoints_out,
    uint8_t* num_codepoints_out
) {
    RELEASE_ASSERT(glyphlist);
    RELEASE_ASSERT(glyph_name);
    RELEASE_ASSERT(codepoints_out);
    RELEASE_ASSERT(num_codepoints_out);

    long int low = 0;
    long int high = (long int)glyph_list_entries_len(glyphlist->entries) - 1;

    while (low <= high) {
        long int mid = low + (high - low) / 2;

        GlyphListEntry entry;
        RELEASE_ASSERT(
            glyph_list_entries_get(glyphlist->entries, (size_t)mid, &entry)
        );

        int cmp = strncmp(entry.name, glyph_name, entry.name_len);
        if (cmp < 0) {
            low = mid + 1;
        } else if (cmp > 0) {
            high = mid - 1;
        } else {
            memcpy(codepoints_out, entry.codepoints, sizeof(uint16_t) * 4);
            *num_codepoints_out = entry.num_codepoints;
            return NULL;
        }
    }

    return PDF_ERROR(
        PDF_ERR_INVALID_GLYPH_NAME,
        "Invalid glyph name `%s`",
        glyph_name
    );
}
