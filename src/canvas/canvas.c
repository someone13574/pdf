#include "canvas/canvas.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "logger/log.h"

#define BMP_HEADER_LEN 14
#define BMP_INFO_HEADER_LEN 40

struct Canvas {
    uint32_t width;
    uint32_t height;
    uint32_t file_size;
    uint8_t* data;
};

static void write_u16(uint8_t* target, uint16_t value) {
    RELEASE_ASSERT(target);

    target[0] = (value >> 0) & 0xff;
    target[1] = (value >> 8) & 0xff;
}

static void write_u32(uint8_t* target, uint32_t value) {
    RELEASE_ASSERT(target);

    target[0] = (value >> 0) & 0xff;
    target[1] = (value >> 8) & 0xff;
    target[2] = (value >> 16) & 0xff;
    target[3] = (value >> 24) & 0xff;
}

static void write_bmp_header(uint8_t* target, uint32_t file_size) {
    RELEASE_ASSERT(target);

    // Header field
    target[0] = 'B';
    target[1] = 'M';

    // File size
    write_u32(target + 2, file_size);

    // Pixel data offset
    write_u32(target + 10, BMP_HEADER_LEN + BMP_INFO_HEADER_LEN);
}

static void
write_bmp_info_header(uint8_t* target, uint32_t width, uint32_t height) {
    RELEASE_ASSERT(target);
    RELEASE_ASSERT((int32_t)width >= 0); // the size is technically signed
    RELEASE_ASSERT((int32_t)height >= 0);

    write_u32(target, 40); // header size
    write_u32(target + 4, width); // width
    write_u32(target + 8, height); // height
    write_u16(target + 12, 1); // color planes
    write_u16(target + 14, 32); // bits per pixel
    write_u32(target + 16, 0); // BI_RGB
    write_u32(target + 20, 0); // image size, can be 0 for BI_RGB
}

Canvas* canvas_new(uint32_t width, uint32_t height, uint32_t rgba) {
    uint32_t file_size =
        BMP_HEADER_LEN + BMP_INFO_HEADER_LEN + width * height * 4;

    LOG_DIAG(
        INFO,
        CANVAS,
        "Creating new %ux%u (%u bytes) canvas with initial color 0x%08" PRIx32,
        width,
        height,
        file_size,
        rgba
    );

    Canvas* canvas = malloc(sizeof(Canvas));
    RELEASE_ASSERT(canvas);

    canvas->width = width;
    canvas->height = height;
    canvas->file_size = file_size;

    canvas->data = calloc(file_size, 1);
    RELEASE_ASSERT(canvas->data);

    write_bmp_header(canvas->data, file_size);
    write_bmp_info_header(canvas->data + BMP_HEADER_LEN, width, height);

    uint8_t r = (rgba >> 24) & 0xff;
    uint8_t g = (rgba >> 16) & 0xff;
    uint8_t b = (rgba >> 8) & 0xff;
    uint8_t a = rgba & 0xff;
    for (uint32_t idx = 0; idx < width * height; idx++) {
        canvas->data[BMP_HEADER_LEN + BMP_INFO_HEADER_LEN + idx * 4] = b;
        canvas->data[BMP_HEADER_LEN + BMP_INFO_HEADER_LEN + idx * 4 + 1] = g;
        canvas->data[BMP_HEADER_LEN + BMP_INFO_HEADER_LEN + idx * 4 + 2] = r;
        canvas->data[BMP_HEADER_LEN + BMP_INFO_HEADER_LEN + idx * 4 + 3] = a;
    }

    return canvas;
}

void canvas_free(Canvas* canvas) {
    RELEASE_ASSERT(canvas);

    LOG_DIAG(INFO, CANVAS, "Freeing canvas");

    free(canvas->data);
    free(canvas);
}

uint32_t canvas_width(Canvas* canvas) {
    RELEASE_ASSERT(canvas);
    return canvas->width;
}

uint32_t canvas_height(Canvas* canvas) {
    RELEASE_ASSERT(canvas);
    return canvas->height;
}

uint32_t canvas_get_rgba(Canvas* canvas, uint32_t x, uint32_t y) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(x < canvas->width);
    RELEASE_ASSERT(y < canvas->height);

    uint32_t pixel_offset = BMP_HEADER_LEN + BMP_INFO_HEADER_LEN
        + ((canvas->height - y - 1) * canvas->width + x) * 4;

    return ((uint32_t)canvas->data[pixel_offset + 2] << 24)
        | ((uint32_t)canvas->data[pixel_offset + 1] << 16)
        | ((uint32_t)canvas->data[pixel_offset] << 8)
        | ((uint32_t)canvas->data[pixel_offset + 3]);
}

void canvas_set_rgba(Canvas* canvas, uint32_t x, uint32_t y, uint32_t rgba) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(x < canvas->width);
    RELEASE_ASSERT(y < canvas->height);

    LOG_DIAG(
        TRACE,
        CANVAS,
        "Setting canvas pixel (%u, %u) to 0x%08" PRIx32,
        x,
        y,
        rgba
    );

    uint32_t pixel_offset = BMP_HEADER_LEN + BMP_INFO_HEADER_LEN
        + ((canvas->height - y - 1) * canvas->width + x) * 4;

    canvas->data[pixel_offset + 2] = (rgba >> 24) & 0xff;
    canvas->data[pixel_offset + 1] = (rgba >> 16) & 0xff;
    canvas->data[pixel_offset] = (rgba >> 8) & 0xff;
    canvas->data[pixel_offset + 3] = rgba & 0xff;
}

bool canvas_write_file(Canvas* canvas, const char* path) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(path);

    LOG_DIAG(INFO, CANVAS, "Writing canvas to `%s`", path);

    FILE* file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    unsigned long written = fwrite(canvas->data, 1, canvas->file_size, file);
    fclose(file);

    return written == canvas->file_size;
}
