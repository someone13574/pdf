#include "filters.h"

#include <stdint.h>
#include <string.h>

#include "arena/arena.h"
#include "codec/zlib.h"
#include "logger/log.h"
#include "pdf_error/error.h"

PdfError* pdf_decode_filtered_stream(
    Arena* arena,
    const char* encoded,
    size_t length,
    PdfOpNameArray filters,
    char** decoded,
    size_t* decoded_len
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(encoded);
    RELEASE_ASSERT(decoded);
    RELEASE_ASSERT(decoded_len);

    if (filters.discriminant && pdf_void_vec_len(filters.value.elements) != 0) {
        Arena* local_arena = arena_new(1024);
        uint8_t* temp = (uint8_t*)encoded;
        size_t temp_len = length;

        for (size_t idx = 0; idx < pdf_void_vec_len(filters.value.elements);
             idx++) {
            void* void_ptr = NULL;
            RELEASE_ASSERT(
                pdf_void_vec_get(filters.value.elements, idx, &void_ptr)
            );

            PdfName* name_ptr = void_ptr;
            LOG_DIAG(DEBUG, OBJECT, "Decoding stream with \"%s\"", *name_ptr);

            if (strcmp(*name_ptr, "ASCIIHexDecode") == 0) {
                uint8_t* out;
                size_t out_len;

                PDF_PROPAGATE(pdf_filter_ascii_hex_decode(
                    local_arena,
                    temp,
                    temp_len,
                    &out,
                    &out_len
                ));

                temp = out;
                temp_len = out_len;
            } else if (strcmp(*name_ptr, "FlateDecode") == 0) {
                Uint8Array* out = NULL;
                PDF_PROPAGATE(
                    decode_zlib_data(local_arena, temp, temp_len, &out)
                );

                temp = uint8_array_get_raw(out, &temp_len);
            } else {
                LOG_TODO("Unimplemented filter: \"%s\"", *name_ptr);
            }
        }

        *decoded = arena_alloc(arena, temp_len);
        memcpy(*decoded, temp, temp_len);
        *decoded_len = temp_len;

        arena_free(local_arena);

        return NULL;
    } else {
        *decoded = arena_alloc(arena, sizeof(char) * (size_t)(length + 1));
        strcpy(*decoded, (char*)encoded);

        return NULL;
    }
}
