#include "filters.h"

#include <stdint.h>
#include <string.h>

#include "arena/arena.h"
#include "codec/zlib.h"
#include "err/error.h"
#include "logger/log.h"

Error* pdf_decode_filtered_stream(
    Arena* arena,
    const uint8_t* encoded,
    size_t length,
    PdfAsNameVecOptional filters,
    uint8_t** decoded,
    size_t* decoded_len
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(encoded);
    RELEASE_ASSERT(decoded);
    RELEASE_ASSERT(decoded_len);

    if (filters.is_some && pdf_name_vec_len(filters.value) != 0) {
        Arena* local_arena = arena_new(1024);
        uint8_t* temp = (uint8_t*)encoded;
        size_t temp_len = length;

        for (size_t idx = 0; idx < pdf_name_vec_len(filters.value); idx++) {
            PdfName name;
            RELEASE_ASSERT(pdf_name_vec_get(filters.value, idx, &name));
            LOG_DIAG(DEBUG, OBJECT, "Decoding stream with \"%s\"", name);

            if (strcmp(name, "ASCIIHexDecode") == 0) {
                uint8_t* out;
                size_t out_len;

                TRY(pdf_filter_ascii_hex_decode(
                    local_arena,
                    temp,
                    temp_len,
                    &out,
                    &out_len
                ));

                temp = out;
                temp_len = out_len;
            } else if (strcmp(name, "FlateDecode") == 0) {
                Uint8Array* out = NULL;
                TRY(decode_zlib_data(local_arena, temp, temp_len, &out));

                temp = uint8_array_get_raw(out, &temp_len);
            } else {
                LOG_TODO("Unimplemented filter: \"%s\"", name);
            }
        }

        *decoded = arena_alloc(arena, temp_len);
        memcpy(*decoded, temp, temp_len);
        *decoded_len = temp_len;

        arena_free(local_arena);

        return NULL;
    } else {
        *decoded = arena_alloc(arena, length);
        memcpy(*decoded, encoded, length);
        *decoded_len = length;

        return NULL;
    }
}
