#include "pdf/content_stream/stream.h"

#include <stdio.h>
#include <string.h>

#include "../ctx.h"
#include "../deserialize.h"
#include "../object.h"
#include "arena/arena.h"
#include "logger/log.h"
#include "operation.h"
#include "operator.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_content_stream(
    const PdfObject* object,
    PdfContentStream* deserialized,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(deserialized);
    RELEASE_ASSERT(resolver);

    PdfObject resolved;
    PDF_PROPAGATE(pdf_resolve_object(resolver, object, &resolved, true));
    if (resolved.type != PDF_OBJECT_TYPE_STREAM) {
        return PDF_ERROR(PDF_ERR_INCORRECT_TYPE, "Expected a stream");
    }

    PdfStream* stream = &resolved.data.stream;

    PdfCtx* ctx = pdf_ctx_new(
        pdf_resolver_arena(resolver),
        stream->stream_bytes,
        stream->decoded_stream_len
    );
    PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));

    PdfObjectVec* operands = pdf_object_vec_new(pdf_resolver_arena(resolver));
    PdfContentOpVec* operations =
        pdf_content_op_vec_new(pdf_resolver_arena(resolver));

    while (pdf_ctx_offset(ctx) != pdf_ctx_buffer_len(ctx)) {
        // Parse operands
        pdf_object_vec_clear(operands);

        PdfObject operand;
        size_t restore_offset = pdf_ctx_offset(ctx);
        PdfError* stop_reason = NULL;

        while (true) {
            stop_reason = pdf_parse_operand_object(
                pdf_resolver_arena(resolver),
                ctx,
                &operand
            );
            if (stop_reason) {
                stop_reason = PDF_ERROR_ADD_CONTEXT_FMT(
                    stop_reason,
                    "Ending operand parsing"
                );
                break;
            }

            PdfObject* operand_alloc =
                arena_alloc(pdf_resolver_arena(resolver), sizeof(PdfObject));
            *operand_alloc = operand;
            pdf_object_vec_push(operands, operand_alloc);

            PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));
            restore_offset = pdf_ctx_offset(ctx);
        }

        PDF_PROPAGATE(pdf_ctx_seek(ctx, restore_offset));

        // Parse operator
        PdfOperator operator = PDF_OPERATOR_UNSET;
        PDF_PROPAGATE(pdf_error_conditional_context(
            pdf_parse_operator(ctx, &operator),
            stop_reason
        ));
        PDF_PROPAGATE(pdf_ctx_require_byte_type(ctx, true, is_pdf_non_regular));
        PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));

        RELEASE_ASSERT(operator != PDF_OPERATOR_UNSET);

        // Deserialize operation
        PDF_PROPAGATE(
            pdf_deserialize_content_op(operator, operands, resolver, operations)
        );
    }

    deserialized->operations = operations;

    return NULL;
}

DESERDE_IMPL_RESOLVABLE(
    PdfContentStreamRef,
    PdfContentStream,
    pdf_content_stream_ref_init,
    pdf_resolve_content_stream,
    pdf_deserialize_content_stream
)

#define DVEC_NAME PdfContentStreamRefVec
#define DVEC_LOWERCASE_NAME pdf_content_stream_ref_vec
#define DVEC_TYPE PdfContentStreamRef
#include "arena/dvec_impl.h"

DESERDE_IMPL_OPTIONAL(
    PdfContentsStreamRefVecOptional,
    pdf_content_stream_ref_vec_op_init
)
