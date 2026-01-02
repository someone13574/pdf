#include "pdf/content_stream/stream.h"

#include <stdio.h>
#include <string.h>

#include "../ctx.h"
#include "../object.h"
#include "err/error.h"
#include "logger/log.h"
#include "operation.h"
#include "operator.h"
#include "pdf/content_stream/operator.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

Error* pdf_deserde_content_stream(
    const PdfObject* object,
    PdfContentStream* deserialized,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(deserialized);
    RELEASE_ASSERT(resolver);

    PdfObject resolved;
    TRY(pdf_resolve_object(resolver, object, &resolved, true));
    if (resolved.type != PDF_OBJECT_TYPE_STREAM) {
        return ERROR(PDF_ERR_INCORRECT_TYPE, "Expected a stream");
    }

    PdfStream* stream = &resolved.data.stream;

    PdfCtx* ctx = pdf_ctx_new(
        pdf_resolver_arena(resolver),
        stream->stream_bytes,
        stream->decoded_stream_len
    );
    TRY(pdf_ctx_consume_whitespace(ctx));

    int compatibility = 0;
    PdfObjectVec* operands = pdf_object_vec_new(pdf_resolver_arena(resolver));
    PdfContentOpVec* operations =
        pdf_content_op_vec_new(pdf_resolver_arena(resolver));

    while (pdf_ctx_offset(ctx) != pdf_ctx_buffer_len(ctx)) {
        // Parse operands
        pdf_object_vec_clear(operands);

        PdfObject operand;
        size_t restore_offset = pdf_ctx_offset(ctx);
        Error* stop_reason = NULL;

        while (true) {
            stop_reason = pdf_parse_operand_object(
                pdf_resolver_arena(resolver),
                ctx,
                &operand
            );
            if (stop_reason) {
                stop_reason = ERROR_ADD_CONTEXT_FMT(
                    stop_reason,
                    "Ending operand parsing"
                );
                break;
            }

            pdf_object_vec_push(operands, operand);

            TRY(pdf_ctx_consume_whitespace(ctx));
            restore_offset = pdf_ctx_offset(ctx);
        }

        TRY(pdf_ctx_seek(ctx, restore_offset));

        // Parse operator
        PdfOperator operator = PDF_OPERATOR_UNSET;
        Error* operator_error = pdf_parse_operator(ctx, &operator);
        if (compatibility > 0 && operator_error) {
            error_free(operator_error);
            pdf_ctx_consume_regular(ctx);
            operator = PDF_OPERATOR_UNSET;
        } else {
            RELEASE_ASSERT(operator != PDF_OPERATOR_UNSET);
        }
        TRY(error_conditional_context(operator_error, stop_reason));

        TRY(pdf_ctx_require_byte_type(ctx, true, is_pdf_non_regular));
        TRY(pdf_ctx_consume_whitespace(ctx));

        if (operator == PDF_OPERATOR_BX) {
            compatibility++;
            RELEASE_ASSERT(compatibility > 0);
        } else if (operator == PDF_OPERATOR_EX) {
            compatibility--;
            RELEASE_ASSERT(compatibility >= 0);
        }

        // Deserialize operation
        if (operator != PDF_OPERATOR_UNSET) {
            TRY(
                pdf_deserde_content_op(operator, operands, resolver, operations)
            );
        }
    }

    deserialized->operations = operations;

    return NULL;
}

PDF_IMPL_RESOLVABLE_FIELD(PdfContentStream, PdfContentStreamRef, content_stream)

#define DVEC_NAME PdfContentStreamRefVec
#define DVEC_LOWERCASE_NAME pdf_content_stream_ref_vec
#define DVEC_TYPE PdfContentStreamRef
#include "arena/dvec_impl.h"

PDF_IMPL_AS_ARRAY_FIELD(
    PdfContentStreamRefVec,
    content_stream_ref_vec,
    content_stream_ref
)

PDF_IMPL_OPTIONAL_FIELD(
    PdfContentStreamRefVec*,
    PdfContentStreamRefVecOptional,
    as_content_stream_ref_vec
)
