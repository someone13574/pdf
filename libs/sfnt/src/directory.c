#include "directory.h"

#include <stdint.h>

#include "arena/arena.h"
#include "err/error.h"
#include "logger/log.h"
#include "parser.h"
#include "sfnt/sfnt.h"

#define DARRAY_NAME SfntDirectoryEntryVec
#define DARRAY_LOWERCASE_NAME sfnt_directory_entry_array
#define DARRAY_TYPE SfntDirectoryEntry
#include "arena/darray_impl.h"

Error*
sfnt_parse_directory_entry(SfntParser* parser, SfntDirectoryEntry* entry) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(entry);

    TRY(sfnt_parser_read_uint32(parser, &entry->tag));
    TRY(sfnt_parser_read_uint32(parser, &entry->checksum));
    TRY(sfnt_parser_read_uint32(parser, &entry->offset));
    TRY(sfnt_parser_read_uint32(parser, &entry->length));

    return NULL;
}

Error* sfnt_parse_directory(
    Arena* arena,
    SfntParser* parser,
    SfntFontDirectory* directory
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(directory);

    parser->offset = 0;
    TRY(sfnt_parser_read_uint32(parser, &directory->scalar_type));
    TRY(sfnt_parser_read_uint16(parser, &directory->num_tables));
    TRY(sfnt_parser_read_uint16(parser, &directory->search_range));
    TRY(sfnt_parser_read_uint16(parser, &directory->entry_selector));
    TRY(sfnt_parser_read_uint16(parser, &directory->range_shift));

    directory->entries =
        sfnt_directory_entry_array_new(arena, directory->num_tables);
    for (size_t idx = 0; idx < directory->num_tables; idx++) {
        SfntDirectoryEntry entry;
        TRY(sfnt_parse_directory_entry(parser, &entry));
        sfnt_directory_entry_array_set(directory->entries, idx, entry);
    }

    return NULL;
}

Error* sfnt_directory_get_entry(
    const SfntFontDirectory* directory,
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
            return NULL;
        }
    }

    return ERROR(
        SFNT_ERR_MISSING_TABLE,
        "Couldn't find the entry for the table `%c%c%c%c` in the directory",
        (tag >> 24) & 0xff,
        (tag >> 16) & 0xff,
        (tag >> 8) & 0xff,
        tag & 0xff
    );
}
