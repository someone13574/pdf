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
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfCIDSystemInfo* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfCIDSystemInfo,
            "Registry",
            registry,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STRING)
        ),
        PDF_FIELD(
            PdfCIDSystemInfo,
            "Ordering",
            ordering,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STRING)
        ),
        PDF_FIELD(
            PdfCIDSystemInfo,
            "Supplement",
            supplement,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
        )
    };

    deserialized->raw_dict = object;
    PDF_PROPAGATE(pdf_deserialize_object(
        deserialized,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        resolver,
        false
    ));

    return NULL;
}

PdfError* pdf_deserialize_cid_system_info_wrapper(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    return pdf_deserialize_cid_system_info(
        object,
        arena,
        resolver,
        deserialized
    );
}

typedef struct {
    size_t start_code;
    size_t len;

    Uint32Array* map;
} CMapTable;

#define DVEC_NAME CMapTableVec
#define DVEC_LOWERCASE_NAME cmap_table_vec
#define DVEC_TYPE CMapTable
#include "arena/dvec_impl.h"

struct PdfCMap {
    Arena* arena;
    CMapTableVec* tables;

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

typedef struct {
    PdfCMap* cmap;
    size_t curr_sink_len;
} CMapPostscriptUserData;

static PdfError* cidinit_begincmap(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);
    return NULL;
}

static PdfError* cidinit_endcmap(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);
    return NULL;
}

static PdfError* cidinit_usecmap(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(postscript_interpreter_user_data(
        interpreter,
        "cmap",
        (void**)&user_data
    ));

    if (user_data->cmap->use_cmap) {
        return PDF_ERROR(
            PDF_ERR_CMAP_ALREADY_DERIVE,
            "Cannot call `usecmap` multiple times"
        );
    }

    PostscriptObject cmap_name;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_NAME,
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

static PdfError* cidinit_begincodespacerange(PostscriptInterpreter* interpreter
) {
    RELEASE_ASSERT(interpreter);

    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(postscript_interpreter_user_data(
        interpreter,
        "cmap",
        (void**)&user_data
    ));

    PostscriptObject length;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_INTEGER,
        true,
        &length
    ));

    if (length.data.integer < 0) {
        return PDF_ERROR(PDF_ERR_POSTSCRIPT_INVALID_LENGTH);
    }

    postscript_interpreter_operand_push(
        interpreter,
        (PostscriptObject
        ) {.type = POSTSCRIPT_OBJECT_SINK,
           .data.sink.list = postscript_object_list_new(
               postscript_interpreter_get_arena(interpreter)
           ),
           .data.sink.type = POSTSCRIPT_SINK_CUSTOM,
           .data.sink.sink_name = "codespacerange",
           .access = POSTSCRIPT_ACCESS_UNLIMITED,
           .literal = true}
    );

    user_data->curr_sink_len = (size_t)length.data.integer;

    return NULL;
}

static PdfError* cidinit_endcodespacerange(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    // Get user data
    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(postscript_interpreter_user_data(
        interpreter,
        "cmap",
        (void**)&user_data
    ));

    // Get sink
    PostscriptObject sink;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_SINK,
        true,
        &sink
    ));

    if (sink.data.sink.type != POSTSCRIPT_SINK_CUSTOM
        || strcmp("codespacerange", sink.data.sink.sink_name) != 0) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
            "codespacerange sink had wrong name"
        );
    }

    if (postscript_object_list_len(sink.data.sink.list)
        != user_data->curr_sink_len * 2) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
            "codespacerange sink had incorrect number of elements"
        );
    }

    // Read table
    for (size_t idx = 0; idx < user_data->curr_sink_len * 2; idx += 2) {
        PostscriptObject start_str;
        PostscriptObject end_str;
        RELEASE_ASSERT(
            postscript_object_list_get(sink.data.sink.list, idx, &start_str)
        );
        RELEASE_ASSERT(
            postscript_object_list_get(sink.data.sink.list, idx + 1, &end_str)
        );

        if (start_str.type != POSTSCRIPT_OBJECT_STRING) {
            return PDF_ERROR(
                PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
                "codespacerange start must be a string"
            );
        }

        if (end_str.type != POSTSCRIPT_OBJECT_STRING) {
            return PDF_ERROR(
                PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
                "codespacerange end must be a string"
            );
        }

        uint64_t start;
        uint64_t end;
        PDF_PROPAGATE(postscript_string_as_uint(start_str.data.string, &start));
        PDF_PROPAGATE(postscript_string_as_uint(end_str.data.string, &end));

        size_t len = (size_t)(end - start) + 1;
        cmap_table_vec_push(
            user_data->cmap->tables,
            (CMapTable
            ) {.start_code = (size_t)start,
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

static PdfError* cidinit_begincidrange(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(postscript_interpreter_user_data(
        interpreter,
        "cmap",
        (void**)&user_data
    ));

    PostscriptObject length;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_INTEGER,
        true,
        &length
    ));

    if (length.data.integer < 0) {
        return PDF_ERROR(PDF_ERR_POSTSCRIPT_INVALID_LENGTH);
    }

    postscript_interpreter_operand_push(
        interpreter,
        (PostscriptObject
        ) {.type = POSTSCRIPT_OBJECT_SINK,
           .data.sink.list = postscript_object_list_new(
               postscript_interpreter_get_arena(interpreter)
           ),
           .data.sink.type = POSTSCRIPT_SINK_CUSTOM,
           .data.sink.sink_name = "cidrange",
           .access = POSTSCRIPT_ACCESS_UNLIMITED,
           .literal = true}
    );

    user_data->curr_sink_len = (size_t)length.data.integer;

    return NULL;
}

static PdfError* cidinit_endcidrange(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    // Get user data
    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(postscript_interpreter_user_data(
        interpreter,
        "cmap",
        (void**)&user_data
    ));

    // Get sink
    PostscriptObject sink;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_SINK,
        true,
        &sink
    ));

    if (sink.data.sink.type != POSTSCRIPT_SINK_CUSTOM
        || strcmp("cidrange", sink.data.sink.sink_name) != 0) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
            "cidrange sink had wrong name"
        );
    }

    if (postscript_object_list_len(sink.data.sink.list)
        != user_data->curr_sink_len * 3) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
            "cidrange sink had incorrect number of elements"
        );
    }

    // Read table
    for (size_t idx = 0; idx < user_data->curr_sink_len * 3; idx += 3) {
        PostscriptObject code_start_str;
        PostscriptObject code_end_str;
        PostscriptObject cid_start;
        RELEASE_ASSERT(postscript_object_list_get(
            sink.data.sink.list,
            idx,
            &code_start_str
        ));
        RELEASE_ASSERT(postscript_object_list_get(
            sink.data.sink.list,
            idx + 1,
            &code_end_str
        ));
        RELEASE_ASSERT(
            postscript_object_list_get(sink.data.sink.list, idx + 2, &cid_start)
        );

        if (code_start_str.type != POSTSCRIPT_OBJECT_STRING) {
            return PDF_ERROR(
                PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
                "cidrange start must be a string"
            );
        }

        if (code_end_str.type != POSTSCRIPT_OBJECT_STRING) {
            return PDF_ERROR(
                PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
                "cidrange end must be a string"
            );
        }

        if (cid_start.type != POSTSCRIPT_OBJECT_INTEGER
            || cid_start.data.integer < 0) {
            return PDF_ERROR(
                PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
                "cidrange cid start must be a positive integer"
            );
        }

        uint64_t code_start;
        uint64_t code_end;
        PDF_PROPAGATE(
            postscript_string_as_uint(code_start_str.data.string, &code_start)
        );
        PDF_PROPAGATE(
            postscript_string_as_uint(code_end_str.data.string, &code_end)
        );

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

PdfError* pdf_parse_cmap(
    Arena* arena,
    const char* data,
    size_t data_len,
    PdfCMap** cmap_out
) {
    RELEASE_ASSERT(data);

    *cmap_out = arena_alloc(arena, sizeof(PdfCMap));
    (*cmap_out)->arena = arena;
    (*cmap_out)->tables = cmap_table_vec_new(arena);
    (*cmap_out)->use_cmap = NULL;

    Arena* interpreter_arena = arena_new(1024);

    PostscriptTokenizer* tokenizer =
        postscript_tokenizer_new(interpreter_arena, data, data_len);
    PostscriptInterpreter* interpreter =
        postscript_interpreter_new(interpreter_arena);
    postscript_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_begincmap,
        "begincmap"
    );
    postscript_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_endcmap,
        "endcmap"
    );
    postscript_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_usecmap,
        "usecmap"
    );
    postscript_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_begincodespacerange,
        "begincodespacerange"
    );
    postscript_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_endcodespacerange,
        "endcodespacerange"
    );
    postscript_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_begincidrange,
        "begincidrange"
    );
    postscript_interpreter_add_operator(
        interpreter,
        "ProcSet",
        "CIDInit",
        cidinit_endcidrange,
        "endcidrange"
    );

    // Push user data
    CMapPostscriptUserData user_data = {.cmap = *cmap_out, .curr_sink_len = 0};
    postscript_interpreter_user_data_push(interpreter, &user_data, "cmap");

    // Interpret cmap
    while (true) {
        bool got_token;
        PostscriptToken token;

        PDF_PROPAGATE(postscript_next_token(tokenizer, &token, &got_token));

        if (!got_token) {
            break;
        }

        PDF_PROPAGATE(postscript_interpret_token(interpreter, token));
    }

    postscript_interpreter_dump(interpreter);
    arena_free(interpreter_arena);

    return NULL;
}

static char*
load_file_to_buffer(Arena* arena, const char* path, size_t* out_size) {
    FILE* file = fopen(path, "rb");
    *out_size = 0;
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long len = ftell(file);
    if (len < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char* buffer = arena_alloc(arena, (size_t)len);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)len, file) != (size_t)len) {
        fclose(file);
        return NULL;
    }
    fclose(file);

    *out_size = (size_t)len;
    return buffer;
}

PdfError* pdf_load_cmap(Arena* arena, char* name, PdfCMap** cmap_out) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(name);
    RELEASE_ASSERT(cmap_out);

    const char* cmap_path = NULL;
    PDF_PROPAGATE(pdf_cmap_name_to_path(name, &cmap_path));

    Arena* file_arena = arena_new(1);

    size_t file_len;
    char* file = load_file_to_buffer(file_arena, cmap_path, &file_len);

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
