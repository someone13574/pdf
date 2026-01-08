#include "directory.h"

#include <stdint.h>

#include "arena/arena.h"
#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/ctx.h"
#include "sfnt/sfnt.h"

#define DARRAY_NAME SfntDirectoryEntryVec
#define DARRAY_LOWERCASE_NAME sfnt_directory_entry_array
#define DARRAY_TYPE SfntDirectoryEntry
#include "arena/darray_impl.h"

static Error*
sfnt_parse_directory_entry(ParseCtx* ctx, SfntDirectoryEntry* entry) {
    RELEASE_ASSERT(entry);

    TRY(parse_ctx_read_u32_be(ctx, &entry->tag));
    TRY(parse_ctx_read_u32_be(ctx, &entry->checksum));
    TRY(parse_ctx_read_u32_be(ctx, &entry->offset));
    TRY(parse_ctx_read_u32_be(ctx, &entry->length));

    LOG_DIAG(
        DEBUG,
        SFNT,
        "Directory entry: `%c%c%c%c`",
        (entry->tag >> 24) & 0xff,
        (entry->tag >> 16) & 0xff,
        (entry->tag >> 8) & 0xff,
        entry->tag & 0xff
    );

    return NULL;
}

Error*
sfnt_parse_directory(Arena* arena, ParseCtx ctx, SfntFontDirectory* directory) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(directory);

    TRY(parse_ctx_seek(&ctx, 0));
    TRY(parse_ctx_read_u32_be(&ctx, &directory->scalar_type));
    TRY(parse_ctx_read_u16_be(&ctx, &directory->num_tables));
    TRY(parse_ctx_read_u16_be(&ctx, &directory->search_range));
    TRY(parse_ctx_read_u16_be(&ctx, &directory->entry_selector));
    TRY(parse_ctx_read_u16_be(&ctx, &directory->range_shift));

    directory->entries =
        sfnt_directory_entry_array_new(arena, directory->num_tables);
    for (size_t idx = 0; idx < directory->num_tables; idx++) {
        SfntDirectoryEntry entry;
        TRY(sfnt_parse_directory_entry(&ctx, &entry));
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
