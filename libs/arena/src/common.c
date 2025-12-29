#include "arena/common.h"

#include <stdio.h>

uint8_t* load_file_to_buffer(Arena* arena, const char* path, size_t* out_size) {
    FILE* file = fopen(path, "rb");
    if (out_size) {
        *out_size = 0;
    }
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long len = ftell(file);
    if (len < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    uint8_t* buffer = arena_alloc(arena, (size_t)len);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)len, file) != (size_t)len) {
        fclose(file);
        return NULL;
    }
    fclose(file);

    if (out_size) {
        *out_size = (size_t)len;
    }
    return buffer;
}

#define DVEC_NAME Uint8Vec
#define DVEC_LOWERCASE_NAME uint8_vec
#define DVEC_TYPE uint8_t
#include "arena/dvec_impl.h"

#define DARRAY_NAME Uint8Array
#define DARRAY_LOWERCASE_NAME uint8_array
#define DARRAY_TYPE uint8_t
#include "arena/darray_impl.h"

#define DARRAY_NAME Uint16Array
#define DARRAY_LOWERCASE_NAME uint16_array
#define DARRAY_TYPE uint16_t
#include "arena/darray_impl.h"

#define DARRAY_NAME Int32Array
#define DARRAY_LOWERCASE_NAME int32_array
#define DARRAY_TYPE int32_t
#include "arena/darray_impl.h"

#define DARRAY_NAME Uint32Array
#define DARRAY_LOWERCASE_NAME uint32_array
#define DARRAY_TYPE uint32_t
#include "arena/darray_impl.h"
