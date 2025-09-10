#include "pdf/fonts/cmap.h"

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

static PdfError* cid_init_begincmap(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    return NULL;
}

PdfError* pdf_parse_cmap(Arena* arena, const char* data, size_t data_len) {
    RELEASE_ASSERT(data);

    PostscriptTokenizer* tokenizer =
        postscript_tokenizer_new(arena, data, data_len);
    PostscriptInterpreter* interpreter = postscript_interpreter_new(arena);
    postscript_interpreter_add_proc(
        interpreter,
        "ProcSet",
        "CIDInit",
        cid_init_begincmap,
        "begincmap"
    );

    while (true) {
        bool got_token;
        PostscriptToken token;

        PDF_PROPAGATE(postscript_next_token(tokenizer, &token, &got_token));

        LOG_DIAG(
            DEBUG,
            CMAP,
            "Got token: `%s`",
            postscript_string_as_cstr(token.text, arena)
        );

        if (!got_token) {
            break;
        }

        PDF_PROPAGATE(postscript_interpret_token(interpreter, token));
    }

    return NULL;
}
