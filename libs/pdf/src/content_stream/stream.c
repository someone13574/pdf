#include <stdio.h>
#include <string.h>

#include "../ctx.h"
#include "../object.h"
#include "arena/arena.h"
#include "logger/log.h"
#include "op.h"
#include "operator.h"
#include "pdf/content_stream.h"
#include "pdf/object.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_content_stream(
    const PdfStream* stream,
    Arena* arena,
    PdfContentStream* deserialized
) {
    RELEASE_ASSERT(stream);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfCtx* ctx =
        pdf_ctx_new(arena, stream->stream_bytes, stream->decoded_stream_len);
    PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));

    PdfObjectVec* operands = pdf_object_vec_new(arena);
    PdfContentOpVec* operations = pdf_content_op_vec_new(arena);

    while (pdf_ctx_offset(ctx) != pdf_ctx_buffer_len(ctx)) {
        // Parse operands
        pdf_object_vec_clear(operands);

        PdfObject operand;
        size_t restore_offset = pdf_ctx_offset(ctx);
        PdfError* stop_reason = NULL;

        while (true) {
            stop_reason = pdf_parse_operand_object(arena, ctx, &operand);
            if (stop_reason) {
                stop_reason = PDF_ERROR_ADD_CONTEXT_FMT(
                    stop_reason,
                    "Ending operand parsing"
                );
                break;
            }

            PdfObject* operand_alloc = arena_alloc(arena, sizeof(PdfObject));
            *operand_alloc = operand;
            pdf_object_vec_push(operands, operand_alloc);

            PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));
            restore_offset = pdf_ctx_offset(ctx);
        }

        PDF_PROPAGATE(pdf_ctx_seek(ctx, restore_offset));

        // Parse operator
        PdfOperator operator;
        PDF_PROPAGATE(pdf_error_conditional_context(
            pdf_parse_operator(ctx, &operator),
            stop_reason
        ));
        PDF_PROPAGATE(pdf_ctx_require_byte_type(ctx, true, is_pdf_non_regular));
        PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));

        // Deserialize operation
        PDF_PROPAGATE(
            pdf_deserialize_content_op(operator, operands, arena, operations)
        );
    }

    deserialized->operations = operations;

    return NULL;
}
