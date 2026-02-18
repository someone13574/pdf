#include "pdf/function.h"

#include <math.h>
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
#include "pdf/stream_dict.h"
#include "pdf/types.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"
#include "postscript/tokenizer.h"
#include "test_helpers.h"

PDF_IMPL_FIELD(PdfFunction, function)

#define DVEC_NAME PdfFunctionVec
#define DVEC_LOWERCASE_NAME pdf_function_vec
#define DVEC_TYPE PdfFunction
#include "arena/dvec_impl.h"

PDF_IMPL_ARRAY_FIELD(PdfFunctionVec, function_vec, function)
PDF_IMPL_AS_ARRAY_FIELD(PdfFunctionVec, function_vec, function)

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
        case 2: {
            if (pdf_number_vec_len(function->domain) != 2) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }
            if (pdf_object_vec_len(io) != 1) {
                return ERROR(PDF_ERR_EXCESS_OPERAND);
            }

            PdfObject input_object;
            RELEASE_ASSERT(pdf_object_vec_get(io, 0, &input_object));

            PdfNumber domain_min;
            PdfNumber domain_max;
            RELEASE_ASSERT(
                pdf_number_vec_get(function->domain, 0, &domain_min)
            );
            RELEASE_ASSERT(
                pdf_number_vec_get(function->domain, 1, &domain_max)
            );

            PdfObject clipped_input;
            TRY(clip_num(input_object, domain_min, domain_max, &clipped_input));

            PdfNumber x_num;
            TRY(pdf_deserde_number(&clipped_input, &x_num, NULL));
            PdfReal x = pdf_number_as_real(x_num);

            size_t output_size = 1;
            if (function->data.type2.c0.is_some) {
                output_size = pdf_number_vec_len(function->data.type2.c0.value);
            }
            if (function->data.type2.c1.is_some) {
                size_t c1_size =
                    pdf_number_vec_len(function->data.type2.c1.value);
                if (function->data.type2.c0.is_some && c1_size != output_size) {
                    return ERROR(PDF_ERR_INCORRECT_TYPE);
                }
                output_size = c1_size;
            }

            if (output_size == 0) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            if (function->range.is_some
                && pdf_number_vec_len(function->range.value)
                       != 2 * output_size) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            PdfReal n = pdf_number_as_real(function->data.type2.n);
            PdfReal x_to_n = pow(x, n);

            pdf_object_vec_clear(io);
            for (size_t idx = 0; idx < output_size; idx++) {
                PdfReal c0 = 0.0;
                if (function->data.type2.c0.is_some) {
                    PdfNumber c0_number;
                    RELEASE_ASSERT(pdf_number_vec_get(
                        function->data.type2.c0.value,
                        idx,
                        &c0_number
                    ));
                    c0 = pdf_number_as_real(c0_number);
                }

                PdfReal c1 = 1.0;
                if (function->data.type2.c1.is_some) {
                    PdfNumber c1_number;
                    RELEASE_ASSERT(pdf_number_vec_get(
                        function->data.type2.c1.value,
                        idx,
                        &c1_number
                    ));
                    c1 = pdf_number_as_real(c1_number);
                }

                PdfObject output = {
                    .type = PDF_OBJECT_TYPE_REAL,
                    .data.real = c0 + x_to_n * (c1 - c0)
                };

                if (function->range.is_some) {
                    PdfNumber range_min;
                    PdfNumber range_max;
                    RELEASE_ASSERT(pdf_number_vec_get(
                        function->range.value,
                        2 * idx,
                        &range_min
                    ));
                    RELEASE_ASSERT(pdf_number_vec_get(
                        function->range.value,
                        2 * idx + 1,
                        &range_max
                    ));

                    PdfObject clipped_output;
                    TRY(
                        clip_num(output, range_min, range_max, &clipped_output)
                    );
                    output = clipped_output;
                }

                pdf_object_vec_push(io, output);
            }

            break;
        }
        case 3: {
            if (pdf_number_vec_len(function->domain) != 2) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }
            if (pdf_object_vec_len(io) != 1) {
                return ERROR(PDF_ERR_EXCESS_OPERAND);
            }

            size_t k = pdf_function_vec_len(function->data.type3.functions);
            if (k == 0) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }
            if (pdf_number_vec_len(function->data.type3.bounds) != k - 1) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }
            if (pdf_number_vec_len(function->data.type3.encode) != 2 * k) {
                return ERROR(PDF_ERR_INCORRECT_TYPE);
            }

            PdfObject input_object;
            RELEASE_ASSERT(pdf_object_vec_get(io, 0, &input_object));

            PdfNumber domain_min_num;
            PdfNumber domain_max_num;
            RELEASE_ASSERT(
                pdf_number_vec_get(function->domain, 0, &domain_min_num)
            );
            RELEASE_ASSERT(
                pdf_number_vec_get(function->domain, 1, &domain_max_num)
            );

            PdfObject clipped_input;
            TRY(clip_num(
                input_object,
                domain_min_num,
                domain_max_num,
                &clipped_input
            ));

            PdfNumber x_num;
            TRY(pdf_deserde_number(&clipped_input, &x_num, NULL));
            PdfReal x = pdf_number_as_real(x_num);

            PdfReal domain_min = pdf_number_as_real(domain_min_num);
            PdfReal domain_max = pdf_number_as_real(domain_max_num);

            size_t selected_idx = k - 1;
            PdfReal selected_low = domain_min;
            PdfReal selected_high = domain_max;
            PdfReal low = domain_min;
            for (size_t idx = 0; idx < k; idx++) {
                PdfReal high = domain_max;
                if (idx + 1 < k) {
                    PdfNumber bound;
                    RELEASE_ASSERT(pdf_number_vec_get(
                        function->data.type3.bounds,
                        idx,
                        &bound
                    ));
                    high = pdf_number_as_real(bound);
                }

                bool in_interval = false;
                if (idx + 1 == k) {
                    in_interval = (x >= low - 1e-9) && (x <= high + 1e-9);
                } else {
                    in_interval = (x >= low - 1e-9) && (x < high);
                }
                if (in_interval) {
                    selected_idx = idx;
                    selected_low = low;
                    selected_high = high;
                    break;
                }

                low = high;
            }

            PdfNumber encode_min_num;
            PdfNumber encode_max_num;
            RELEASE_ASSERT(pdf_number_vec_get(
                function->data.type3.encode,
                2 * selected_idx,
                &encode_min_num
            ));
            RELEASE_ASSERT(pdf_number_vec_get(
                function->data.type3.encode,
                2 * selected_idx + 1,
                &encode_max_num
            ));

            PdfReal encode_min = pdf_number_as_real(encode_min_num);
            PdfReal encode_max = pdf_number_as_real(encode_max_num);

            PdfReal mapped_x = encode_min;
            if (fabs(selected_high - selected_low) > 1e-9) {
                mapped_x = encode_min
                         + ((x - selected_low) * (encode_max - encode_min))
                               / (selected_high - selected_low);
            }

            PdfFunction selected_function;
            RELEASE_ASSERT(pdf_function_vec_get(
                function->data.type3.functions,
                selected_idx,
                &selected_function
            ));

            pdf_object_vec_clear(io);
            pdf_object_vec_push(
                io,
                (PdfObject) {.type = PDF_OBJECT_TYPE_REAL,
                             .data.real = mapped_x}
            );
            TRY(pdf_run_function(&selected_function, arena, io));

            if (function->range.is_some) {
                size_t output_len = pdf_object_vec_len(io);
                if (output_len == 0) {
                    return ERROR(PDF_ERR_INCORRECT_TYPE);
                }
                if (pdf_number_vec_len(function->range.value)
                    != 2 * output_len) {
                    return ERROR(PDF_ERR_INCORRECT_TYPE);
                }

                PdfObject clipped_outputs[output_len];
                for (size_t idx = 0; idx < output_len; idx++) {
                    PdfObject output;
                    RELEASE_ASSERT(pdf_object_vec_get(io, idx, &output));

                    PdfNumber range_min;
                    PdfNumber range_max;
                    RELEASE_ASSERT(pdf_number_vec_get(
                        function->range.value,
                        2 * idx,
                        &range_min
                    ));
                    RELEASE_ASSERT(pdf_number_vec_get(
                        function->range.value,
                        2 * idx + 1,
                        &range_max
                    ));

                    TRY(clip_num(
                        output,
                        range_min,
                        range_max,
                        &clipped_outputs[idx]
                    ));
                }

                pdf_object_vec_clear(io);
                for (size_t idx = 0; idx < output_len; idx++) {
                    pdf_object_vec_push(io, clipped_outputs[idx]);
                }
            }

            break;
        }
        case 4: {
            for (size_t idx = 0; idx < pdf_object_vec_len(io); idx++) {
                PdfObject pdf_operand;
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
                TRY(clip_num(pdf_operand, min, max, &clipped));

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

                if (function->range.is_some) {
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

                    PdfObject clipped;
                    TRY(clip_num(converted, min, max, &clipped));
                    pdf_object_vec_push(io, clipped);
                } else {
                    pdf_object_vec_push(io, converted);
                }
            }

            break;
        }
        default: {
            LOG_TODO("Function type %d", function->function_type);
        }
    }

    return NULL;
}

#ifdef TEST

#include "test/test.h"

TEST_FUNC(test_pdf_function_type2) {
    Arena* arena = arena_new(256);
    uint8_t buffer[] = "10 0 obj\n"
                       "<< /FunctionType 2\n"
                       "/Domain [0.0 1.0]\n"
                       "/C0 [0.0 0.0]\n"
                       "/C1 [1.0 0.5]\n"
                       "/N 2.0\n"
                       ">>\n endobj";
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);

    PdfObject object;
    TEST_REQUIRE(pdf_parse_object(resolver, &object, false));

    PdfFunction func;
    TEST_REQUIRE(pdf_deserde_function(&object, &func, resolver));

    PdfObjectVec* io = pdf_object_vec_new(arena);
    pdf_object_vec_push(
        io,
        (PdfObject) {.type = PDF_OBJECT_TYPE_REAL, .data.real = 0.5}
    );
    TEST_REQUIRE(pdf_run_function(&func, arena, io));

    TEST_ASSERT_EQ(pdf_object_vec_len(io), (size_t)2);

    PdfObject output_0;
    RELEASE_ASSERT(pdf_object_vec_get(io, 0, &output_0));
    TEST_ASSERT_EQ((int)output_0.type, (int)PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ(output_0.data.real, 0.25);

    PdfObject output_1;
    RELEASE_ASSERT(pdf_object_vec_get(io, 1, &output_1));
    TEST_ASSERT_EQ((int)output_1.type, (int)PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ(output_1.data.real, 0.125);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_pdf_function_type3) {
    Arena* arena = arena_new(1024);
    uint8_t buffer[] = "10 0 obj\n"
                       "<< /FunctionType 3\n"
                       "/Domain [0.0 1.0]\n"
                       "/Functions [\n"
                       "<< /FunctionType 2 /Domain [0.0 1.0] /C0 [0.0] /C1 "
                       "[1.0] /N 1.0 >>\n"
                       "<< /FunctionType 2 /Domain [0.0 1.0] /C0 [0.0] /C1 "
                       "[0.0] /N 1.0 >>\n"
                       "]\n"
                       "/Bounds [0.5]\n"
                       "/Encode [0.0 1.0 0.0 1.0]\n"
                       ">>\n endobj";
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);

    PdfObject object;
    TEST_REQUIRE(pdf_parse_object(resolver, &object, false));

    PdfFunction func;
    TEST_REQUIRE(pdf_deserde_function(&object, &func, resolver));

    PdfObjectVec* io = pdf_object_vec_new(arena);
    pdf_object_vec_push(
        io,
        (PdfObject) {.type = PDF_OBJECT_TYPE_REAL, .data.real = 0.25}
    );
    TEST_REQUIRE(pdf_run_function(&func, arena, io));

    TEST_ASSERT_EQ(pdf_object_vec_len(io), (size_t)1);
    PdfObject output;
    RELEASE_ASSERT(pdf_object_vec_get(io, 0, &output));
    TEST_ASSERT_EQ((int)output.type, (int)PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ(output.data.real, 0.5);

    pdf_object_vec_clear(io);
    pdf_object_vec_push(
        io,
        (PdfObject) {.type = PDF_OBJECT_TYPE_REAL, .data.real = 0.75}
    );
    TEST_REQUIRE(pdf_run_function(&func, arena, io));

    TEST_ASSERT_EQ(pdf_object_vec_len(io), (size_t)1);
    RELEASE_ASSERT(pdf_object_vec_get(io, 0, &output));
    TEST_ASSERT_EQ((int)output.type, (int)PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ(output.data.real, 0.0);

    return TEST_RESULT_PASS;
}

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

    PdfObjectVec* io = pdf_object_vec_new(arena);
    pdf_object_vec_push(
        io,
        (PdfObject) {.type = PDF_OBJECT_TYPE_REAL, .data.real = 0.25}
    );
    pdf_object_vec_push(
        io,
        (PdfObject) {.type = PDF_OBJECT_TYPE_REAL, .data.real = 0.5}
    );
    TEST_REQUIRE(pdf_run_function(&func, arena, io));

    PdfObject out;
    TEST_ASSERT(pdf_object_vec_pop(io, &out));

    TEST_ASSERT_EQ((int)out.type, (int)PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ(out.data.real, 0.5);

    return TEST_RESULT_PASS;
}

#endif // TEST
