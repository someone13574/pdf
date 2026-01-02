#include "pdf/function.h"

#include <stdint.h>
#include <stdio.h>

#include "arena/arena.h"
#include "ctx.h"
#include "err/error.h"
#include "logger/log.h"
#include "object.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/stream.h"
#include "pdf/types.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"
#include "postscript/tokenizer.h"
#include "test_helpers.h"

Error* pdf_deserde_function(
    const PdfObject* object,
    PdfFunction* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfObject resolved;
    TRY(pdf_resolve_object(resolver, object, &resolved, true));

    PdfFieldDescriptor fields[] = {
        pdf_integer_field("FunctionType", &target_ptr->function_type),
        pdf_number_vec_field("Domain", &target_ptr->domain),
        pdf_number_vec_optional_field("Range", &target_ptr->range)
    };

    if (resolved.type == PDF_OBJECT_TYPE_DICT) {
        TRY(pdf_deserde_fields(
            &resolved,
            fields,
            sizeof(fields) / sizeof(PdfFieldDescriptor),
            true,
            resolver,
            "Function"
        ));
    } else if (resolved.type == PDF_OBJECT_TYPE_STREAM) {
        TRY(pdf_deserde_fields(
            resolved.data.stream.stream_dict->raw_dict,
            fields,
            sizeof(fields) / sizeof(PdfFieldDescriptor),
            true,
            resolver,
            "Function"
        ));
    } else {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Functions must be a stream or dict"
        );
    }

    switch (target_ptr->function_type) {
        case 2: {
            PdfFieldDescriptor specific_fields[] = {
                pdf_ignored_field("FunctionType", NULL),
                pdf_ignored_field("Domain", NULL),
                pdf_ignored_field("Range", NULL),
                pdf_number_vec_optional_field("C0", &target_ptr->data.type2.c0),
                pdf_number_vec_optional_field("C1", &target_ptr->data.type2.c1),
                pdf_number_field("N", &target_ptr->data.type2.n)
            };

            TRY(pdf_deserde_fields(
                object,
                specific_fields,
                sizeof(specific_fields) / sizeof(PdfFieldDescriptor),
                false,
                resolver,
                "Type3 PdfFunction"
            ));
            break;
        }
        case 3: {
            PdfFieldDescriptor specific_fields[] = {
                pdf_ignored_field("FunctionType", NULL),
                pdf_ignored_field("Domain", NULL),
                pdf_ignored_field("Range", NULL),
                pdf_function_vec_field(
                    "Functions",
                    &target_ptr->data.type3.functions
                ),
                pdf_number_vec_field("Bounds", &target_ptr->data.type3.bounds),
                pdf_number_vec_field("Encode", &target_ptr->data.type3.encode)
            };

            TRY(pdf_deserde_fields(
                object,
                specific_fields,
                sizeof(specific_fields) / sizeof(PdfFieldDescriptor),
                false,
                resolver,
                "Type3 PdfFunction"
            ));

            if (!target_ptr->data.type3.bounds) {
                // TODO: Remove ugly workaround. Deserialization needs another
                // rewrite.
                target_ptr->data.type3.bounds =
                    pdf_number_vec_new(pdf_resolver_arena(resolver));
            }

            size_t k = pdf_function_vec_len(target_ptr->data.type3.functions);
            if (k == 0) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            if (pdf_number_vec_len(target_ptr->data.type3.bounds) != k - 1) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            if (pdf_number_vec_len(target_ptr->data.type3.encode) != 2 * k) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            break;
        }
        case 4: {
            if (resolved.type != PDF_OBJECT_TYPE_STREAM) {
                return ERROR(
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

            TRY(ps_interpret_token(
                interpreter,
                (PSToken) {.type = PS_TOKEN_LIT_NAME, .data.name = "func"}
            ));

            TRY(ps_interpret_tokens(interpreter, tokenizer));

            TRY(ps_interpret_token(
                interpreter,
                (PSToken) {.type = PS_TOKEN_EXE_NAME, .data.name = "def"}
            ));

            target_ptr->data.type4 = interpreter;
            break;
        }
        default: {
            LOG_TODO("Function type %d", target_ptr->function_type);
        }
    }

    return NULL;
}

static Error*
clip_num(PdfObject object, PdfNumber min, PdfNumber max, PdfObject* out) {
    PdfNumber val;
    switch (object.type) {
        case PDF_OBJECT_TYPE_INTEGER: {
            val.type = PDF_NUMBER_TYPE_INTEGER;
            val.value.integer = object.data.integer;
            break;
        }
        case PDF_OBJECT_TYPE_REAL: {
            val.type = PDF_NUMBER_TYPE_REAL;
            val.value.real = object.data.real;
            break;
        }
        case PDF_OBJECT_TYPE_BOOLEAN: {
            *out = object;
            return NULL;
        }
        default: {
            return ERROR(PDF_ERR_INCORRECT_TYPE);
        }
    }

    switch (pdf_number_cmp(val, min)) {
        case -1: {
            *out = pdf_number_as_object(min);
            return NULL;
        }
        case 0: {
            *out = object;
            break;
        }
        case 1: {
            break;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }

    if (pdf_number_cmp(val, max) <= 0) {
        *out = object;
    } else {
        *out = pdf_number_as_object(max);
    }

    return NULL;
}

Error*
pdf_run_function(const PdfFunction* function, Arena* arena, PdfObjectVec* io) {
    RELEASE_ASSERT(function);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(io);

    switch (function->function_type) {
        case 4: {
            for (size_t idx = 0; idx < pdf_object_vec_len(io); idx++) {
                PdfObject* pdf_operand = NULL;
                RELEASE_ASSERT(pdf_object_vec_get(io, idx, &pdf_operand));

                PdfObject clipped;
                PdfNumber min;
                PdfNumber max;
                if (!pdf_number_vec_get(function->domain, 2 * idx, &min)
                    || !pdf_number_vec_get(
                        function->domain,
                        2 * idx + 1,
                        &max
                    )) {
                    return ERROR(PDF_ERR_EXCESS_OPERAND);
                }
                TRY(clip_num(*pdf_operand, min, max, &clipped));

                PSObject operand = {
                    .literal = true,
                    .access = PS_ACCESS_UNLIMITED
                };
                if (clipped.type == PDF_OBJECT_TYPE_INTEGER) {
                    operand.type = PS_OBJECT_INTEGER;
                    operand.data.integer = clipped.data.integer;
                } else if (clipped.type == PDF_OBJECT_TYPE_REAL) {
                    operand.type = PS_OBJECT_REAL;
                    operand.data.real = clipped.data.real;
                } else if (clipped.type == PDF_OBJECT_TYPE_BOOLEAN) {
                    operand.type = PS_OBJECT_BOOLEAN;
                    operand.data.boolean = clipped.data.boolean;
                } else {
                    return ERROR(PDF_ERR_INCORRECT_TYPE);
                }

                TRY(ps_interpret_object(function->data.type4, operand));
            }

            TRY(ps_interpret_token(
                function->data.type4,
                (PSToken) {.type = PS_TOKEN_EXE_NAME, .data.name = "func"}
            ));

            pdf_object_vec_clear(io);

            PSObjectList* stack = ps_interpreter_stack(function->data.type4);
            for (size_t idx = 0; idx < ps_object_list_len(stack); idx++) {
                PSObject output;
                RELEASE_ASSERT(ps_object_list_get(stack, idx, &output));

                PdfObject converted;
                if (output.type == PS_OBJECT_INTEGER) {
                    converted.type = PDF_OBJECT_TYPE_INTEGER;
                    converted.data.integer = output.data.integer;
                } else if (output.type == PS_OBJECT_REAL) {
                    converted.type = PDF_OBJECT_TYPE_REAL;
                    converted.data.real = output.data.real;
                } else if (output.type == PS_OBJECT_BOOLEAN) {
                    converted.type = PDF_OBJECT_TYPE_BOOLEAN;
                    converted.data.boolean = output.data.boolean;
                } else {
                    return ERROR(PS_ERR_OPERAND_TYPE);
                }

                PdfObject* object = arena_alloc(arena, sizeof(PdfObject));

                if (function->range.has_value) {
                    PdfNumber min, max;
                    if (!pdf_number_vec_get(
                            function->range.value,
                            2 * idx,
                            &min
                        )
                        || !pdf_number_vec_get(
                            function->range.value,
                            2 * idx + 1,
                            &max
                        )) {
                        return ERROR(PDF_ERR_EXCESS_OPERAND);
                    }
                    TRY(clip_num(converted, min, max, object));
                } else {
                    *object = converted;
                }
                pdf_object_vec_push(io, object);
            }

            break;
        }
        default: {
            LOG_TODO("Function type %d", function->function_type);
        }
    }

    return NULL;
}

DESERDE_IMPL_TRAMPOLINE(pdf_deserde_function_trampoline, pdf_deserde_function)

#define DVEC_NAME PdfFunctionVec
#define DVEC_LOWERCASE_NAME pdf_function_vec
#define DVEC_TYPE PdfFunction
#include "arena/dvec_impl.h"

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
    TEST_REQUIRE(pdf_parse_object(resolver, &object, false));

    PdfFunction func;
    TEST_REQUIRE(pdf_deserde_function(&object, &func, resolver));

    PdfObject* a = arena_alloc(arena, sizeof(PdfObject));
    PdfObject* b = arena_alloc(arena, sizeof(PdfObject));
    a->type = PDF_OBJECT_TYPE_REAL;
    a->data.real = 0.25;
    b->type = PDF_OBJECT_TYPE_REAL;
    b->data.real = 0.5;

    PdfObjectVec* io = pdf_object_vec_new(arena);
    pdf_object_vec_push(io, a);
    pdf_object_vec_push(io, b);
    TEST_REQUIRE(pdf_run_function(&func, arena, io));

    PdfObject* out = NULL;
    TEST_ASSERT(pdf_object_vec_pop(io, &out));

    TEST_ASSERT_EQ((int)out->type, (int)PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ(out->data.real, 0.5);

    return TEST_RESULT_PASS;
}

#endif // TEST
