#include "pdf/function.h"

#include <stdint.h>
#include <stdio.h>

#include "arena/arena.h"
#include "ctx.h"
#include "deserialize.h"
#include "logger/log.h"
#include "object.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"
#include "postscript/tokenizer.h"
#include "test_helpers.h"

PdfError* pdf_deserialize_function(
    const PdfObject* object,
    PdfFunction* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfObject resolved;
    PDF_PROPAGATE(pdf_resolve_object(resolver, object, &resolved, true));

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "FunctionType",
            &target_ptr->type,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(
            "Domain",
            &target_ptr->domain,
            PDF_DESERDE_ARRAY(
                pdf_number_vec_push_uninit,
                PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
            )
        ),
        PDF_FIELD(
            "Range",
            &target_ptr->range,
            PDF_DESERDE_OPTIONAL(
                pdf_number_vec_op_init,
                PDF_DESERDE_ARRAY(
                    pdf_number_vec_push_uninit,
                    PDF_DESERDE_CUSTOM(pdf_deserialize_number_trampoline)
                )
            )
        )
    };

    if (resolved.type == PDF_OBJECT_TYPE_DICT) {
        PDF_PROPAGATE(pdf_deserialize_dict(
            &resolved,
            fields,
            sizeof(fields) / sizeof(PdfFieldDescriptor),
            true,
            resolver,
            "Function"
        ));
    } else if (resolved.type == PDF_OBJECT_TYPE_STREAM) {
        PDF_PROPAGATE(pdf_deserialize_dict(
            resolved.data.stream.stream_dict->raw_dict,
            fields,
            sizeof(fields) / sizeof(PdfFieldDescriptor),
            true,
            resolver,
            "Function"
        ));
    } else {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Functions must be a stream or dict"
        );
    }

    switch (target_ptr->type) {
        case 4: {
            if (resolved.type != PDF_OBJECT_TYPE_STREAM) {
                return PDF_ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "Type4 function must be a stream"
                );
            }

            PSInterpreter* interpreter =
                ps_interpreter_new(pdf_resolver_arena(resolver));
            PSTokenizer* tokenizer = ps_tokenizer_new(
                pdf_resolver_arena(resolver),
                resolved.data.stream.stream_bytes,
                resolved.data.stream.decoded_stream_len
            );

            PDF_PROPAGATE(ps_interpret_token(
                interpreter,
                (PSToken) {.type = PS_TOKEN_LIT_NAME, .data.name = "func"}
            ));

            PDF_PROPAGATE(ps_interpret_tokens(interpreter, tokenizer));

            PDF_PROPAGATE(ps_interpret_token(
                interpreter,
                (PSToken) {.type = PS_TOKEN_EXE_NAME, .data.name = "def"}
            ));

            target_ptr->data.type4 = interpreter;
            break;
        }
        default: {
            LOG_TODO("Function type %d", target_ptr->type);
        }
    }

    return NULL;
}

PdfError*
pdf_run_function(const PdfFunction* function, Arena* arena, PdfObjectVec* io) {
    RELEASE_ASSERT(function);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(io);

    switch (function->type) {
        case 4: {
            for (size_t idx = 0; idx < pdf_object_vec_len(io); idx++) {
                PdfObject* pdf_operand = NULL;
                RELEASE_ASSERT(pdf_object_vec_get(io, idx, &pdf_operand));

                PSObject operand;
                PDF_PROPAGATE(
                    pdf_object_into_postscript(pdf_operand, &operand)
                );

                PDF_PROPAGATE(
                    ps_interpret_object(function->data.type4, operand)
                );
            }

            PDF_PROPAGATE(ps_interpret_token(
                function->data.type4,
                (PSToken) {.type = PS_TOKEN_EXE_NAME, .data.name = "func"}
            ));

            pdf_object_vec_clear(io);

            PSObjectList* stack = ps_interpreter_stack(function->data.type4);
            for (size_t idx = 0; idx < ps_object_list_len(stack); idx++) {
                PSObject output;
                RELEASE_ASSERT(ps_object_list_get(stack, idx, &output));

                PdfObject converted;
                pdf_object_from_postscript(output, &converted);

                PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
                *object = converted;

                pdf_object_vec_push(io, object);
            }

            break;
        }
        default: {
            LOG_TODO("Function type %d", function->type);
        }
    }

    return NULL;
}

#ifdef TEST

#include "test/test.h"

TEST_FUNC(test_pdf_function) {
    Arena* arena = arena_new(128);
    uint8_t buffer[] = "10 0 obj\n"
                       "<< /FunctionType 4\n"
                       "/Domain [-1.0 1.0 -1.0 1.0]\n"
                       "/Range [-1.0 1.0]\n"
                       "/Length 48\n"
                       ">>\n stream\n"
                       "{ 360 mul sin\n"
                       "2 div\n"
                       "exch 360 mul sin\n"
                       "2 div\n"
                       "add\n"
                       "}\n"
                       "endstream\n endobj";
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);

    PdfObject object;
    TEST_PDF_REQUIRE(pdf_parse_object(resolver, &object, false));

    PdfFunction func;
    TEST_PDF_REQUIRE(pdf_deserialize_function(&object, &func, resolver));

    PdfObject* a = arena_alloc(arena, sizeof(PdfObject));
    PdfObject* b = arena_alloc(arena, sizeof(PdfObject));
    a->type = PDF_OBJECT_TYPE_REAL;
    a->data.real = 0.25;
    b->type = PDF_OBJECT_TYPE_REAL;
    b->data.real = 0.5;

    PdfObjectVec* io = pdf_object_vec_new(arena);
    pdf_object_vec_push(io, a);
    pdf_object_vec_push(io, b);
    TEST_PDF_REQUIRE(pdf_run_function(&func, arena, io));

    PdfObject* out = NULL;
    TEST_ASSERT(pdf_object_vec_pop(io, &out));

    TEST_ASSERT_EQ((int)out->type, (int)PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ(out->data.real, 0.5);

    return TEST_RESULT_PASS;
}

#endif // TEST
