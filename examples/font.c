#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena/arena.h"
#include "arena/common.h"
#include "canvas/canvas.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "logger/log.h"
#include "sfnt/glyph.h"
#include "sfnt/sfnt.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Arena* arena = arena_new(4096);

    size_t buffer_len;
    uint8_t* buffer = load_file_to_buffer(
        arena,
        "assets/fonts-urw-base35/fonts/NimbusSans-Regular.ttf",
        &buffer_len
    );

    SfntFont* font;
    REQUIRE(sfnt_font_new(arena, buffer, buffer_len, &font));

    SfntGlyph glyph;
    REQUIRE(sfnt_get_glyph_for_cid(font, '%', &glyph));

    Canvas* canvas = canvas_new_scalable(arena, 2000, 2000, 0xffffffff, 1.0);
    GeomMat3 transform =
        geom_mat3_new(1.0, 0.0, 0.0, 0.0, -1.0, 0.0, 500.0, 1500.0, 1.0);
    sfnt_glyph_render(
        canvas,
        &glyph,
        transform,
        (CanvasBrush) {.enable_fill = true,
                       .enable_stroke = false,
                       .fill_rgba = 0x000000ff}
    );
    canvas_write_file(canvas, "glyph.svg");

    LOG_DIAG(INFO, EXAMPLE, "Finished");

    arena_free(arena);
    return 0;
}
