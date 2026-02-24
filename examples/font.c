#include <stdint.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "logger/log.h"
#include "parse_ctx/ctx.h"
#include "sfnt/glyph.h"
#include "sfnt/sfnt.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Arena* arena = arena_new(4096);

    ParseCtx ctx = parse_ctx_from_file(
        arena,
        "assets/fonts-urw-base35/fonts/NimbusSans-Regular.ttf"
    );

    SfntFont font;
    REQUIRE(sfnt_font_new(arena, ctx, &font));

    SfntGlyph glyph;
    REQUIRE(sfnt_get_glyph_for_cid(&font, '%', &glyph));

    Canvas* canvas = canvas_new_scalable(
        arena,
        2000,
        2000,
        rgba_new(1.0, 1.0, 1.0, 1.0),
        1.0
    );
    GeomMat3 transform =
        geom_mat3_new(1.0, 0.0, 0.0, 0.0, -1.0, 0.0, 500.0, 1500.0, 1.0);
    sfnt_glyph_render(
        canvas,
        &glyph,
        transform,
        (CanvasBrush) {.enable_fill = true,
                       .enable_stroke = false,
                       .fill_rgba = rgba_new(0.0, 0.0, 0.0, 1.0)}
    );
    canvas_write_file(canvas, "glyph.svg");

    LOG_DIAG(INFO, EXAMPLE, "Finished");

    arena_free(arena);
    return 0;
}
