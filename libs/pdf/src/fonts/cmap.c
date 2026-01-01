#include "pdf/fonts/cmap.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../deserialize.h"
#include "arena/arena.h"
#include "arena/common.h"
#include "cmap_paths.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"
#include "postscript/tokenizer.h"

PdfError* pdf_deserialize_cid_system_info(
    const PdfObject* object,
    PdfCIDSystemInfo* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Registry",
            &target_ptr->registry,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_STRING)
        ),
        PDF_FIELD(
            "Ordering",
            &target_ptr->ordering,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_STRING)
        ),
        PDF_FIELD(
            "Supplement",
            &target_ptr->supplement,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
        )
    };

    PDF_PROPAGATE(pdf_deserialize_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfCIDSystemInfo"
    ));

    return NULL;
}

DESERDE_IMPL_TRAMPOLINE(
    pdf_deserialize_cid_system_info_trampoline,
    pdf_deserialize_cid_system_info
)

typedef struct {
    size_t start_code;
    size_t len;

    Uint32Array* map;
} CMapTable;

#define DVEC_NAME CMapTableVec
#define DVEC_LOWERCASE_NAME cmap_table_vec
#define DVEC_TYPE CMapTable
#include "arena/dvec_impl.h"

typedef struct {
    uint64_t src;
    uint64_t dst; // TODO: allow names
} CMapBfEntry;

#define DVEC_NAME CMapBfEntryVec
#define DVEC_LOWERCASE_NAME cmap_bf_entry_vec
#define DVEC_TYPE CMapBfEntry
#include "arena/dvec_impl.h"

struct PdfCMap {
    Arena* arena;
    CMapTableVec* tables;
    CMapBfEntryVec* bfchar;

    PdfCMap* use_cmap;
};

static PdfError*
cmap_get_entry(PdfCMap* cmap, size_t codepoint, uint32_t** cid_entry_out) {
    RELEASE_ASSERT(cmap);
    RELEASE_ASSERT(cid_entry_out);

    for (size_t table_idx = 0; table_idx < cmap_table_vec_len(cmap->tables);
         table_idx++) {
        CMapTable table;
        RELEASE_ASSERT(cmap_table_vec_get(cmap->tables, table_idx, &table));

        if (codepoint < table.start_code
            || codepoint > table.start_code + table.len) {
            continue;
        }

        RELEASE_ASSERT(uint32_array_get_ptr(
            table.map,
            codepoint - table.start_code,
            cid_entry_out
        ));
        return NULL;
    }

    if (cmap->use_cmap) {
        PDF_PROPAGATE(cmap_get_entry(cmap->use_cmap, codepoint, cid_entry_out));
        return NULL;
    }

    return PDF_ERROR(
        PDF_ERR_CMAP_INVALID_CODEPOINT,
        "Couldn't find entry for codepoint %zu",
        codepoint
    );
}

PdfError*
pdf_cmap_get_cid(PdfCMap* cmap, uint32_t codepoint, uint32_t* cid_out) {
    RELEASE_ASSERT(cmap);
    RELEASE_ASSERT(cid_out);

    uint32_t* entry = NULL;
    PDF_PROPAGATE(cmap_get_entry(cmap, (size_t)codepoint, &entry));

    if (*entry == UINT32_MAX && cmap->use_cmap) {
        PDF_PROPAGATE(cmap_get_entry(cmap->use_cmap, codepoint, &entry));
    }

    if (*entry == UINT32_MAX) {
        return PDF_ERROR(
            PDF_ERR_CMAP_INVALID_CODEPOINT,
            "In-range codepoint wasn't set by cmap"
        );
    }

    *cid_out = *entry;

    return NULL;
}

PdfError*
pdf_cmap_get_unicode(PdfCMap* cmap, uint32_t cid, uint32_t* unicode_out) {
    RELEASE_ASSERT(cmap);
    RELEASE_ASSERT(unicode_out);

    for (size_t idx = 0; idx < cmap_bf_entry_vec_len(cmap->bfchar); idx++) {
        CMapBfEntry entry;
        RELEASE_ASSERT(cmap_bf_entry_vec_get(cmap->bfchar, idx, &entry));

        if (entry.src == cid) {
            *unicode_out = (uint32_t)entry.dst;
            return NULL;
        }
    }

    return PDF_ERROR(
        PDF_ERR_CMAP_INVALID_CODEPOINT,
        "Couldn't find unicode for cid %u",
        cid
    );
}

typedef struct {
    PdfCMap* cmap;
    size_t curr_sink_len;
} CMapPostscriptUserData;

static PdfError* cidinit_begincmap(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);
    return NULL;
}

static PdfError* cidinit_endcmap(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);
    return NULL;
}

static PdfError* cidinit_usecmap(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(
        ps_interpreter_user_data(interpreter, "cmap", (void**)&user_data)
    );

    if (user_data->cmap->use_cmap) {
        return PDF_ERROR(
            PDF_ERR_CMAP_ALREADY_DERIVE,
            "Cannot call `usecmap` multiple times"
        );
    }

    PSObject cmap_name;
    PDF_PROPAGATE(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_NAME,
        true,
        &cmap_name
    ));

    PDF_PROPAGATE(
        pdf_load_cmap(
            user_data->cmap->arena,
            cmap_name.data.name,
            &user_data->cmap->use_cmap
        ),
        "Error occurred while running `usecmap`"
    );

    return NULL;
}

static PdfError* cidinit_begincodespacerange(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(
        ps_interpreter_user_data(interpreter, "cmap", (void**)&user_data)
    );

    PSObject length;
    PDF_PROPAGATE(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_INTEGER,
        true,
        &length
    ));

    if (length.data.integer < 0) {
        return PDF_ERROR(PS_ERR_INVALID_LENGTH);
    }

    ps_interpreter_operand_push(
        interpreter,
        (PSObject) {.type = PS_OBJECT_SINK,
                    .data.sink.list = ps_object_list_new(
                        ps_interpreter_get_arena(interpreter)
                    ),
                    .data.sink.type = PS_SINK_CUSTOM,
                    .data.sink.sink_name = "codespacerange",
                    .access = PS_ACCESS_UNLIMITED,
                    .literal = true}
    );

    user_data->curr_sink_len = (size_t)length.data.integer;

    return NULL;
}

static PdfError* cidinit_endcodespacerange(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    // Get user data
    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(
        ps_interpreter_user_data(interpreter, "cmap", (void**)&user_data)
    );

    // Get sink
    PSObject sink;
    PDF_PROPAGATE(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_SINK,
        true,
        &sink
    ));

    if (sink.data.sink.type != PS_SINK_CUSTOM
        || strcmp("codespacerange", sink.data.sink.sink_name) != 0) {
        return PDF_ERROR(
            PS_ERR_OPERAND_TYPE,
            "codespacerange sink had wrong name"
        );
    }

    if (ps_object_list_len(sink.data.sink.list)
        != user_data->curr_sink_len * 2) {
        return PDF_ERROR(
            PS_ERR_OPERAND_TYPE,
            "codespacerange sink had incorrect number of elements"
        );
    }

    // Read table
    for (size_t idx = 0; idx < user_data->curr_sink_len * 2; idx += 2) {
        PSObject start_str;
        PSObject end_str;
        RELEASE_ASSERT(
            ps_object_list_get(sink.data.sink.list, idx, &start_str)
        );
        RELEASE_ASSERT(
            ps_object_list_get(sink.data.sink.list, idx + 1, &end_str)
        );

        if (start_str.type != PS_OBJECT_STRING) {
            return PDF_ERROR(
                PS_ERR_OPERAND_TYPE,
                "codespacerange start must be a string"
            );
        }

        if (end_str.type != PS_OBJECT_STRING) {
            return PDF_ERROR(
                PS_ERR_OPERAND_TYPE,
                "codespacerange end must be a string"
            );
        }

        uint64_t start;
        uint64_t end;
        PDF_PROPAGATE(ps_string_as_uint(start_str.data.string, &start));
        PDF_PROPAGATE(ps_string_as_uint(end_str.data.string, &end));

        size_t len = (size_t)(end - start) + 1;
        cmap_table_vec_push(
            user_data->cmap->tables,
            (CMapTable) {.start_code = (size_t)start,
                         .len = len,
                         .map = uint32_array_new_init(
                             user_data->cmap->arena,
                             len,
                             UINT32_MAX
                         )}
        );
    }

    return NULL;
}

static PdfError* cidinit_begincidrange(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(
        ps_interpreter_user_data(interpreter, "cmap", (void**)&user_data)
    );

    PSObject length;
    PDF_PROPAGATE(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_INTEGER,
        true,
        &length
    ));

    if (length.data.integer < 0) {
        return PDF_ERROR(PS_ERR_INVALID_LENGTH);
    }

    ps_interpreter_operand_push(
        interpreter,
        (PSObject) {.type = PS_OBJECT_SINK,
                    .data.sink.list = ps_object_list_new(
                        ps_interpreter_get_arena(interpreter)
                    ),
                    .data.sink.type = PS_SINK_CUSTOM,
                    .data.sink.sink_name = "cidrange",
                    .access = PS_ACCESS_UNLIMITED,
                    .literal = true}
    );

    user_data->curr_sink_len = (size_t)length.data.integer;

    return NULL;
}

static PdfError* cidinit_endcidrange(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    // Get user data
    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(
        ps_interpreter_user_data(interpreter, "cmap", (void**)&user_data)
    );

    // Get sink
    PSObject sink;
    PDF_PROPAGATE(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_SINK,
        true,
        &sink
    ));

    if (sink.data.sink.type != PS_SINK_CUSTOM
        || strcmp("cidrange", sink.data.sink.sink_name) != 0) {
        return PDF_ERROR(PS_ERR_OPERAND_TYPE, "cidrange sink had wrong name");
    }

    if (ps_object_list_len(sink.data.sink.list)
        != user_data->curr_sink_len * 3) {
        return PDF_ERROR(
            PS_ERR_OPERAND_TYPE,
            "cidrange sink had incorrect number of elements"
        );
    }

    // Read table
    for (size_t idx = 0; idx < user_data->curr_sink_len * 3; idx += 3) {
        PSObject code_start_str;
        PSObject code_end_str;
        PSObject cid_start;
        RELEASE_ASSERT(
            ps_object_list_get(sink.data.sink.list, idx, &code_start_str)
        );
        RELEASE_ASSERT(
            ps_object_list_get(sink.data.sink.list, idx + 1, &code_end_str)
        );
        RELEASE_ASSERT(
            ps_object_list_get(sink.data.sink.list, idx + 2, &cid_start)
        );

        if (code_start_str.type != PS_OBJECT_STRING) {
            return PDF_ERROR(
                PS_ERR_OPERAND_TYPE,
                "cidrange start must be a string"
            );
        }

        if (code_end_str.type != PS_OBJECT_STRING) {
            return PDF_ERROR(
                PS_ERR_OPERAND_TYPE,
                "cidrange end must be a string"
            );
        }

        if (cid_start.type != PS_OBJECT_INTEGER || cid_start.data.integer < 0) {
            return PDF_ERROR(
                PS_ERR_OPERAND_TYPE,
                "cidrange cid start must be a positive integer"
            );
        }

        uint64_t code_start;
        uint64_t code_end;
        PDF_PROPAGATE(
            ps_string_as_uint(code_start_str.data.string, &code_start)
        );
        PDF_PROPAGATE(ps_string_as_uint(code_end_str.data.string, &code_end));

        for (size_t codepoint = (size_t)code_start,
                    cid = (size_t)cid_start.data.integer;
             codepoint <= (size_t)code_end;
             codepoint++, cid++) {
            uint32_t* entry = NULL;
            PDF_PROPAGATE(cmap_get_entry(user_data->cmap, codepoint, &entry));

            *entry = (uint32_t)cid;
        }
    }

    return NULL;
}

static PdfError* cidinit_beginbfchar(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(
        ps_interpreter_user_data(interpreter, "cmap", (void**)&user_data)
    );

    PSObject length;
    PDF_PROPAGATE(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_INTEGER,
        true,
        &length
    ));

    if (length.data.integer < 0) {
        return PDF_ERROR(PS_ERR_INVALID_LENGTH);
    }

    ps_interpreter_operand_push(
        interpreter,
        (PSObject) {.type = PS_OBJECT_SINK,
                    .data.sink.list = ps_object_list_new(
                        ps_interpreter_get_arena(interpreter)
                    ),
                    .data.sink.type = PS_SINK_CUSTOM,
                    .data.sink.sink_name = "bfchar",
                    .access = PS_ACCESS_UNLIMITED,
                    .literal = true}
    );

    user_data->curr_sink_len = (size_t)length.data.integer;

    return NULL;
}

static PdfError* cidinit_endbfchar(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    // Get user data
    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(
        ps_interpreter_user_data(interpreter, "cmap", (void**)&user_data)
    );

    // Get sink
    PSObject sink;
    PDF_PROPAGATE(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_SINK,
        true,
        &sink
    ));

    if (sink.data.sink.type != PS_SINK_CUSTOM
        || strcmp("bfchar", sink.data.sink.sink_name) != 0) {
        return PDF_ERROR(PS_ERR_OPERAND_TYPE, "cidrange sink had wrong name");
    }

    if (ps_object_list_len(sink.data.sink.list)
        != user_data->curr_sink_len * 2) {
        return PDF_ERROR(
            PS_ERR_OPERAND_TYPE,
            "bfchar sink had incorrect number of elements"
        );
    }

    // Read table
    for (size_t idx = 0; idx < user_data->curr_sink_len * 2; idx += 2) {
        PSObject key_str;
        PSObject value_str;
        RELEASE_ASSERT(ps_object_list_get(sink.data.sink.list, idx, &key_str));
        RELEASE_ASSERT(
            ps_object_list_get(sink.data.sink.list, idx + 1, &value_str)
        );

        if (key_str.type != PS_OBJECT_STRING) {
            return PDF_ERROR(
                PS_ERR_OPERAND_TYPE,
                "bfchar key must be a string"
            );
        }

        if (value_str.type != PS_OBJECT_STRING) {
            // TODO: allow names as well
            return PDF_ERROR(
                PS_ERR_OPERAND_TYPE,
                "bfchar value must be a string"
            );
        }

        uint64_t key_int;
        PDF_PROPAGATE(ps_string_as_uint(key_str.data.string, &key_int));
        uint64_t value_int;
        PDF_PROPAGATE(ps_string_as_uint(value_str.data.string, &value_int));

        cmap_bf_entry_vec_push(
            user_data->cmap->bfchar,
            (CMapBfEntry) {.src = key_int, .dst = value_int}
        );
    }

    return NULL;
}

PdfError* pdf_parse_cmap(
    Arena* arena,
    const uint8_t* data,
    size_t data_len,
    PdfCMap** cmap_out
) {
    RELEASE_ASSERT(data);

    *cmap_out = arena_alloc(arena, sizeof(PdfCMap));
    (*cmap_out)->arena = arena;
    (*cmap_out)->tables = cmap_table_vec_new(arena);
    (*cmap_out)->bfchar = cmap_bf_entry_vec_new(arena);
    (*cmap_out)->use_cmap = NULL;

    Arena* interpreter_arena = arena_new(1024);

    PSTokenizer* tokenizer =
        ps_tokenizer_new(interpreter_arena, data, data_len);
    PSInterpreter* interpreter = ps_interpreter_new(interpreter_arena);
    ps_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_begincmap,
        "begincmap"
    );
    ps_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_endcmap,
        "endcmap"
    );
    ps_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_usecmap,
        "usecmap"
    );
    ps_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_begincodespacerange,
        "begincodespacerange"
    );
    ps_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_endcodespacerange,
        "endcodespacerange"
    );
    ps_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_begincidrange,
        "begincidrange"
    );
    ps_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_endcidrange,
        "endcidrange"
    );
    ps_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_beginbfchar,
        "beginbfchar"
    );
    ps_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_endbfchar,
        "endbfchar"
    );

    // Push user data
    CMapPostscriptUserData user_data = {.cmap = *cmap_out, .curr_sink_len = 0};
    ps_interpreter_user_data_push(interpreter, &user_data, "cmap");

    // Interpret cmap
    PDF_PROPAGATE(ps_interpret_tokens(interpreter, tokenizer));

    arena_free(interpreter_arena);

    return NULL;
}

PdfError* pdf_load_cmap(Arena* arena, char* name, PdfCMap** cmap_out) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(name);
    RELEASE_ASSERT(cmap_out);

    const char* cmap_path = NULL;
    PDF_PROPAGATE(pdf_cmap_name_to_path(name, &cmap_path));

    Arena* file_arena = arena_new(1);

    size_t file_len;
    uint8_t* file = load_file_to_buffer(file_arena, cmap_path, &file_len);

    PDF_PROPAGATE(pdf_parse_cmap(arena, file, file_len, cmap_out));

    arena_free(file_arena);
    return NULL;
}

typedef struct {
    char* name;
    PdfCMap* cmap;
} CMapCacheEntry;

#define DVEC_NAME PdfCMapCacheVec
#define DVEC_LOWERCASE_NAME pdf_cmap_cache_vec
#define DVEC_TYPE CMapCacheEntry
#include "arena/dvec_impl.h"

struct PdfCMapCache {
    Arena* arena;
    PdfCMapCacheVec* vec;
};

PdfCMapCache* pdf_cmap_cache_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    PdfCMapCache* cache = arena_alloc(arena, sizeof(PdfCMapCache));
    cache->arena = arena;
    cache->vec = pdf_cmap_cache_vec_new(arena);

    return cache;
}

PdfError*
pdf_cmap_cache_get(PdfCMapCache* cache, char* name, PdfCMap** cmap_out) {
    RELEASE_ASSERT(cache);
    RELEASE_ASSERT(name);
    RELEASE_ASSERT(cmap_out);

    for (size_t idx = 0; idx < pdf_cmap_cache_vec_len(cache->vec); idx++) {
        CMapCacheEntry cache_entry;
        RELEASE_ASSERT(pdf_cmap_cache_vec_get(cache->vec, idx, &cache_entry));

        if (strcmp(cache_entry.name, name) == 0) {
            *cmap_out = cache_entry.cmap;
            return NULL;
        }
    }

    CMapCacheEntry new_entry = {.name = name, .cmap = NULL};
    PDF_PROPAGATE(pdf_load_cmap(cache->arena, name, &new_entry.cmap));
    pdf_cmap_cache_vec_push(cache->vec, new_entry);
    *cmap_out = new_entry.cmap;

    return NULL;
}
