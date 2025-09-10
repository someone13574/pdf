#include "pdf/fonts/cmap.h"

#include <stdbool.h>
#include <string.h>

#include "../deserialize.h"
#include "arena/arena.h"
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
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STRING)
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
    size_t codespacerange_len;
} CMapPostscriptUserData;

static PdfError* cidinit_begincmap(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    CMapPostscriptUserData* user_data = arena_alloc(
        postscript_interpreter_get_arena(interpreter),
        sizeof(CMapPostscriptUserData)
    );
    postscript_interpreter_user_data_push(interpreter, user_data, "cmap");

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

    user_data->codespacerange_len = (size_t)length.data.integer;

    return NULL;
}

static PdfError* cidinit_endcodespacerange(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    CMapPostscriptUserData* user_data = NULL;
    PDF_PROPAGATE(postscript_interpreter_user_data(
        interpreter,
        "cmap",
        (void**)&user_data
    ));

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
        != user_data->codespacerange_len * 2) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
            "codespacerange sink had incorrect number of elements"
        );
    }

    return NULL;
}

PdfError* pdf_parse_cmap(Arena* arena, const char* data, size_t data_len) {
    RELEASE_ASSERT(data);

    PostscriptTokenizer* tokenizer =
        postscript_tokenizer_new(arena, data, data_len);
    PostscriptInterpreter* interpreter = postscript_interpreter_new(arena);
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

    while (true) {
        bool got_token;
        PostscriptToken token;

        PDF_PROPAGATE(postscript_next_token(tokenizer, &token, &got_token));

        if (!got_token) {
            break;
        }

        PDF_PROPAGATE(postscript_interpret_token(interpreter, token));
    }

    return NULL;
}
