#include "arena/arena.h"
#include "canvas/canvas.h"
#include "canvas/path_builder.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Arena* arena = arena_new(1024);
    PathBuilder* builder = path_builder_new(arena);

    // path_builder_new_contour(builder, 460.0, 230.0);
    // path_builder_line_to(builder, 260.0, 360.0);
    // path_builder_line_to(builder, 340.0, 620.0);
    // path_builder_line_to(builder, 620.0, 600.0);
    // path_builder_line_to(builder, 700.0, 450.0);
    // path_builder_end_contour(builder);

    // path_builder_new_contour(builder, 650.0, 285.0);
    // path_builder_line_to(builder, 520.0, 510.0);
    // path_builder_line_to(builder, 765.0, 480.0);
    // path_builder_end_contour(builder);

    // path_builder_new_contour(builder, 560.0, 140.0);
    // path_builder_line_to(builder, 600.0, 680.0);
    // path_builder_line_to(builder, 720.0, 720.0);
    // path_builder_end_contour(builder);

    // path_builder_new_contour(builder, 300.0, 250.0);
    // path_builder_line_to(builder, 440.0, 380.0);
    // path_builder_line_to(builder, 500.0, 670.0);
    // path_builder_line_to(builder, 760.0, 325.0);
    // path_builder_end_contour(builder);

    //

    path_builder_new_contour(builder, 175.0, 150.0);
    path_builder_line_to(builder, 250.0, 370.0);
    path_builder_line_to(builder, 330.0, 215.0);
    path_builder_line_to(builder, 440.0, 455.0);
    path_builder_line_to(builder, 530.0, 165.0);
    path_builder_line_to(builder, 830.0, 310.0);
    path_builder_line_to(builder, 880.0, 560.0);
    path_builder_line_to(builder, 700.0, 420.0);
    path_builder_line_to(builder, 550.0, 710.0);
    path_builder_line_to(builder, 350.0, 600.0);
    path_builder_line_to(builder, 220.0, 720.0);
    path_builder_line_to(builder, 175.0, 490.0);
    path_builder_line_to(builder, 65.0, 290.0);
    path_builder_end_contour(builder);

    path_builder_new_contour(builder, 610.0, 275.0);
    path_builder_line_to(builder, 550.0, 330.0);
    path_builder_line_to(builder, 820.0, 430.0);
    path_builder_end_contour(builder);

    // path_builder_new_contour(builder, 650.0, 120.0);
    // path_builder_line_to(builder, 660.0, 650.0);
    // path_builder_line_to(builder, 930.0, 290.0);
    // path_builder_end_contour(builder);

    Canvas* canvas = path_builder_render(builder, 1000, 900);
    canvas_write_file(canvas, "tessellation.bmp");

    arena_free(arena);
    return 0;
}
