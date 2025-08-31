#include "render/render.h"

#include "canvas/canvas.h"
#include "graphics_state.h"
#include "logger/log.h"
#include "pdf/content_op.h"
#include "pdf/content_stream.h"
#include "pdf/types.h"
#include "text_state.h"

typedef struct {
    GraphicsState graphics_state;
    TextObjectState text_object_state;
} RenderState;

static void process_content_stream(
    RenderState* state,
    PdfContentStream* content_stream,
    Canvas* canvas
) {
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(content_stream);
    RELEASE_ASSERT(canvas);

    for (size_t idx = 0;
         idx < pdf_content_op_vec_len(content_stream->operations);
         idx++) {
        PdfContentOp op;
        RELEASE_ASSERT(
            pdf_content_op_vec_get(content_stream->operations, idx, &op)
        );

        switch (op.kind) {
            default: {
                LOG_TODO("Unimplemented content operation %d", op.kind);
            }
        }
    }
}

Canvas* render_page(Arena* arena, PdfPage* page) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(page);

    PdfReal width = pdf_number_as_real(page->media_box.upper_right_x)
        - pdf_number_as_real(page->media_box.lower_left_x);
    PdfReal height = pdf_number_as_real(page->media_box.upper_right_y)
        - pdf_number_as_real(page->media_box.lower_left_y);

    Canvas* canvas =
        canvas_new_scalable(arena, (uint32_t)width, (uint32_t)height);

    if (page->contents.discriminant) {
        RenderState state = {
            .graphics_state = graphics_state_default(),
            .text_object_state = text_object_state_default()
        };

        for (size_t contents_idx = 0;
             contents_idx < pdf_void_vec_len(page->contents.value.elements);
             contents_idx++) {
            void* content_ptr;
            RELEASE_ASSERT(pdf_void_vec_get(
                page->contents.value.elements,
                contents_idx,
                &content_ptr
            ));
            PdfStream* content = content_ptr;

            PdfContentStream stream;
            PDF_REQUIRE(pdf_deserialize_content_stream(content, arena, &stream)
            );

            process_content_stream(&state, &stream, canvas);
        }
    }

    return canvas;
}
