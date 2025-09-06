#include <stdint.h>
#include <string.h>

#include "../ctx.h"
#include "arena/arena.h"
#include "filters.h"
#include "logger/log.h"
#include "pdf_error/error.h"

static bool char_to_hex(uint8_t c, int* out) {
    if (c >= '0' && c <= '9') {
        *out = c - '0';
        return true;
    } else if (c >= 'A' && c <= 'F') {
        *out = c - 'A' + 10;
        return true;
    } else if (c >= 'a' && c <= 'f') {
        *out = c - 'a' + 10;
        return true;
    } else {
        return false;
    }
}

PdfError* pdf_filter_ascii_hex_decode(
    Arena* arena,
    const uint8_t* stream,
    size_t stream_len,
    uint8_t** decoded,
    size_t* decoded_len
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(stream);
    RELEASE_ASSERT(decoded);

    *decoded = arena_alloc(arena, stream_len / 2 + 2);

    *decoded_len = 0;
    bool upper = true;
    for (size_t offset = 0; offset < stream_len; offset++) {
        uint8_t c = stream[offset];
        if (is_pdf_whitespace(c)) {
            continue;
        }

        if (c == '>') {
            if (!upper) {
                *decoded_len += 1;
            }

            break;
        }

        int hex;
        if (!char_to_hex(c, &hex)) {
            return PDF_ERROR(
                PDF_ERR_FILTER_ASCII_HEX_INVALID,
                "Unexpected character `%c` in ASCIIHexDecode filter stream",
                c
            );
        }

        if (upper) {
            (*decoded)[*decoded_len] = (uint8_t)(hex << 4);
            upper = false;
        } else {
            (*decoded)[*decoded_len] |= (uint8_t)hex;
            *decoded_len += 1;
            upper = true;
        }
    }

    return NULL;
}

#ifdef TEST
#include "test/test.h"

TEST_FUNC(test_ascii_hex_decode_basic) {
    Arena* arena = arena_new(1024);

    const char* encoded = "68656C6C6F20776F726C64";

    uint8_t* decoded = NULL;
    size_t decoded_len;
    TEST_PDF_REQUIRE(pdf_filter_ascii_hex_decode(
        arena,
        (uint8_t*)encoded,
        strlen(encoded),
        &decoded,
        &decoded_len
    ));

    decoded[decoded_len] = '\0';

    TEST_ASSERT_EQ((size_t)11, decoded_len);
    TEST_ASSERT_EQ("hello world", (char*)decoded);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ascii_hex_decode_spaces) {
    Arena* arena = arena_new(1024);

    const char* encoded = " 686  56  C6C6F 207 76F 726C6 4";

    uint8_t* decoded = NULL;
    size_t decoded_len;
    TEST_PDF_REQUIRE(pdf_filter_ascii_hex_decode(
        arena,
        (uint8_t*)encoded,
        strlen(encoded),
        &decoded,
        &decoded_len
    ));

    decoded[decoded_len] = '\0';

    TEST_ASSERT_EQ((size_t)11, decoded_len);
    TEST_ASSERT_EQ("hello world", (char*)decoded);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ascii_hex_decode_even_eod) {
    Arena* arena = arena_new(1024);

    const char* encoded = "68656C6C6F20>776F726C64";

    uint8_t* decoded = NULL;
    size_t decoded_len;
    TEST_PDF_REQUIRE(pdf_filter_ascii_hex_decode(
        arena,
        (uint8_t*)encoded,
        strlen(encoded),
        &decoded,
        &decoded_len
    ));

    decoded[decoded_len] = '\0';

    TEST_ASSERT_EQ((size_t)6, decoded_len);
    TEST_ASSERT_EQ("hello ", (char*)decoded);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ascii_hex_decode_odd_eod) {
    Arena* arena = arena_new(1024);

    const char* encoded = "68656C6C6F2>0776F726C64";

    uint8_t* decoded = NULL;
    size_t decoded_len;
    TEST_PDF_REQUIRE(pdf_filter_ascii_hex_decode(
        arena,
        (uint8_t*)encoded,
        strlen(encoded),
        &decoded,
        &decoded_len
    ));

    decoded[decoded_len] = '\0';

    TEST_ASSERT_EQ((size_t)6, decoded_len);
    TEST_ASSERT_EQ("hello ", (char*)decoded);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ascii_hex_decode_err) {
    Arena* arena = arena_new(1024);

    const char* encoded = "68656C6C6xF20776F726C64";

    uint8_t* decoded = NULL;
    size_t decoded_len;
    TEST_PDF_REQUIRE_ERR(
        pdf_filter_ascii_hex_decode(
            arena,
            (uint8_t*)encoded,
            strlen(encoded),
            &decoded,
            &decoded_len
        ),
        PDF_ERR_FILTER_ASCII_HEX_INVALID
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
