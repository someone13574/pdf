#include "arena/arena.h"
#include "canvas/canvas.h"
#include "canvas/dcel_builder.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Arena* arena = arena_new(1024);
    DcelBuilder* builder = dcel_builder_new(arena);

    dcel_builder_new_contour(builder, 175.0, 150.0);
    dcel_builder_line_to(builder, 250.0, 370.0);
    dcel_builder_line_to(builder, 330.0, 215.0);
    dcel_builder_line_to(builder, 440.0, 455.0);
    dcel_builder_line_to(builder, 530.0, 165.0);
    dcel_builder_line_to(builder, 830.0, 310.0);
    dcel_builder_line_to(builder, 880.0, 560.0);
    dcel_builder_line_to(builder, 700.0, 420.0);
    dcel_builder_line_to(builder, 550.0, 710.0);
    dcel_builder_line_to(builder, 350.0, 600.0);
    dcel_builder_line_to(builder, 220.0, 720.0);
    dcel_builder_line_to(builder, 175.0, 490.0);
    dcel_builder_line_to(builder, 65.0, 290.0);
    dcel_builder_end_contour(builder);

    dcel_builder_new_contour(builder, 630.0, 255.0);
    dcel_builder_line_to(builder, 510.0, 400.0);
    dcel_builder_line_to(builder, 600.0, 350.0);
    dcel_builder_line_to(builder, 785.0, 390.0);
    dcel_builder_end_contour(builder);

    // dcel_builder_new_contour(builder, 630.0, 255.0);
    // dcel_builder_line_to(builder, 785.0, 390.0);
    // dcel_builder_line_to(builder, 600.0, 350.0);
    // dcel_builder_line_to(builder, 510.0, 400.0);
    // dcel_builder_end_contour(builder);

    Canvas* canvas = dcel_builder_render(builder, 1000, 900);
    canvas_write_file(canvas, "tessellation.bmp");

    arena_free(arena);
    return 0;
}
