#include "directory.h"

#include <stdint.h>

#include "arena/arena.h"
#include "log.h"
#include "parser.h"
#include "pdf/result.h"
#include "sfnt/sfnt.h"

#define DARRAY_NAME SfntDirectoryEntryVec
#define DARRAY_LOWERCASE_NAME sfnt_directory_entry_array
#define DARRAY_TYPE SfntDirectoryEntry
#include "../arena/darray_impl.h"

PdfResult
sfnt_parse_directory_entry(SfntParser* parser, SfntDirectoryEntry* entry) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(entry);

    PDF_PROPAGATE(sfnt_parser_read_uint32(parser, &entry->tag));
    PDF_PROPAGATE(sfnt_parser_read_uint32(parser, &entry->checksum));
    PDF_PROPAGATE(sfnt_parser_read_uint32(parser, &entry->offset));
    PDF_PROPAGATE(sfnt_parser_read_uint32(parser, &entry->length));

    return PDF_OK;
}

PdfResult sfnt_parse_directory(
    Arena* arena,
    SfntParser* parser,
    SfntFontDirectory* directory
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(directory);

    parser->offset = 0;
    PDF_PROPAGATE(sfnt_parser_read_uint32(parser, &directory->scalar_type));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &directory->num_tables));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &directory->search_range));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &directory->entry_selector));
    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &directory->range_shift));

    directory->entries =
        sfnt_directory_entry_array_new(arena, directory->num_tables);
    for (size_t idx = 0; idx < directory->num_tables; idx++) {
        SfntDirectoryEntry entry;
        PDF_PROPAGATE(sfnt_parse_directory_entry(parser, &entry));
        sfnt_directory_entry_array_set(directory->entries, idx, entry);
    }

    return PDF_OK;
}

PdfResult sfnt_directory_get_entry(
    SfntFontDirectory* directory,
    uint32_t tag,
    SfntDirectoryEntry** entry
) {
    RELEASE_ASSERT(directory);
    RELEASE_ASSERT(entry);

    for (size_t idx = 0; idx < directory->num_tables; idx++) {
        SfntDirectoryEntry* directory_entry;
        RELEASE_ASSERT(sfnt_directory_entry_array_get_ptr(
            directory->entries,
            idx,
            &directory_entry
        ));
        if (directory_entry->tag == tag) {
            *entry = directory_entry;
            return PDF_OK;
        }
    }

    return PDF_ERR_SFNT_MISSING_TABLE;
}
