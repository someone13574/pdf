#include <stdio.h>
#include <string.h>

#include "../ctx.h"
#include "../object.h"
#include "arena/arena.h"
#include "log.h"
#include "op.h"
#include "operator.h"
#include "pdf/content_stream.h"
#include "pdf/error.h"
#include "pdf/object.h"

PdfError* pdf_deserialize_content_stream(
    PdfStream* stream,
    Arena* arena,
    PdfContentStream* deserialized
) {
    RELEASE_ASSERT(stream);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(deserialized);

    PdfCtx* ctx =
        pdf_ctx_new(arena, stream->stream_bytes, strlen(stream->stream_bytes));
    PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));

    PdfObjectVec* operands = pdf_object_vec_new(arena);
    PdfContentOpVec* operations = pdf_content_op_vec_new(arena);

    while (pdf_ctx_offset(ctx) != pdf_ctx_buffer_len(ctx)) {
        // Parse operands
        pdf_object_vec_clear(operands);

        PdfObject operand;
        size_t restore_offset = pdf_ctx_offset(ctx);
        while (
            pdf_error_free_is_ok(pdf_parse_operand_object(arena, ctx, &operand))
        ) {
            PdfObject* operand_alloc = arena_alloc(arena, sizeof(PdfObject));
            *operand_alloc = operand;
            pdf_object_vec_push(operands, operand_alloc);

            PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));
            restore_offset = pdf_ctx_offset(ctx);
        }

        PDF_PROPAGATE(pdf_ctx_seek(ctx, restore_offset));

        // Parse operator
        PdfOperator operator;
        PDF_PROPAGATE(pdf_parse_operator(ctx, &operator));
        PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, true, is_pdf_non_regular));
        PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));

        // Deserialize operation
        PDF_REQUIRE(
            pdf_deserialize_content_op(operator, operands, arena, operations)
        );
    }

    deserialized->operations = operations;

    return NULL;
}
