#include "codec/zlib.h"

#include <stdint.h>
#include <stdio.h>

#include "adler32.h"
#include "arena/common.h"
#include "bitstream.h"
#include "deflate.h"
#include "logger/log.h"
#include "pdf_error/error.h"

typedef struct {
    uint32_t method;
    uint32_t info;
} ZlibCMF;

static PdfError* zlib_decode_cmf(BitStream* bitstream, ZlibCMF* cmf_out) {
    RELEASE_ASSERT(bitstream);
    RELEASE_ASSERT(cmf_out);

    PDF_PROPAGATE(bitstream_read_n(bitstream, 4, &cmf_out->method));
    PDF_PROPAGATE(bitstream_read_n(bitstream, 4, &cmf_out->info));

    switch (cmf_out->method) {
        case 8: {
            break;
        }
        case 15: {
            return PDF_ERROR(CODEC_ERR_ZLIB_RESERVED_CM);
        }
        default: {
            return PDF_ERROR(CODEC_ERR_ZLIB_INVALID_CM);
        }
    }

    return NULL;
}

typedef struct {
    uint32_t check;
    uint32_t dict_enable;
    uint32_t level;
    uint32_t dict;
} ZlibFLG;

static PdfError* zlib_decode_flg(BitStream* bitstream, ZlibFLG* flg_out) {
    RELEASE_ASSERT(bitstream);
    RELEASE_ASSERT(flg_out);

    PDF_PROPAGATE(bitstream_read_n(bitstream, 5, &flg_out->check));
    PDF_PROPAGATE(bitstream_next(bitstream, &flg_out->dict_enable));
    PDF_PROPAGATE(bitstream_read_n(bitstream, 2, &flg_out->level));

    if (flg_out->dict_enable != 0) {
        // Adler32 of the preset dictionary
        PDF_PROPAGATE(bitstream_read_n(bitstream, 32, &flg_out->dict));
    } else {
        flg_out->dict = 0;
    }

    return NULL;
}

PdfError* decode_zlib_data(
    Arena* arena,
    const uint8_t* data,
    size_t data_len,
    Uint8Array** decoded_bytes
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(data);
    RELEASE_ASSERT(decoded_bytes);

    BitStream bitstream = bitstream_new(data, data_len);

    // Read header
    ZlibCMF cmf;
    PDF_PROPAGATE(zlib_decode_cmf(&bitstream, &cmf));
    // size_t window_size = 1 << (cmf.info + 8);

    ZlibFLG flg;
    PDF_PROPAGATE(zlib_decode_flg(&bitstream, &flg));

    uint32_t check_val = (cmf.info << 12) | (cmf.method << 8) | (flg.level << 6)
                       | (flg.dict_enable << 5) | (flg.check);
    if (check_val % 31 != 0) {
        return PDF_ERROR(
            CODEC_ERR_ZLIB_INVALID_FCHECK,
            "Zlib stream has invalid FCHECK"
        );
    } else {
        LOG_DIAG(TRACE, CODEC, "FCHECK valid");
    }

    // Get preset dictionary
    if (flg.dict_enable) {
        LOG_TODO("Zlib preset dictionaries not yet supported");
    }

    // Deflate stream
    PDF_PROPAGATE(decode_deflate_data(arena, &bitstream, decoded_bytes));

    // Validate checksum
    size_t decoded_len;
    uint8_t* raw_decoded = uint8_array_get_raw(*decoded_bytes, &decoded_len);
    Adler32Sum computed_checksum =
        adler32_compute_checksum(raw_decoded, decoded_len);

    Adler32Sum stored_checksum;
    bitstream_align_byte(&bitstream);
    PDF_PROPAGATE(bitstream_read_n(&bitstream, 32, &stored_checksum));
    adler32_swap_endianness(&stored_checksum);

    if (computed_checksum != stored_checksum) {
        return PDF_ERROR(
            CODEC_ERR_ZLIB_INVALID_CHECKSUM,
            "Adler32 of decoded deflate stream didn't match (0x%08x (computed) != 0x%08x)",
            (unsigned int)computed_checksum,
            (unsigned int)stored_checksum
        );
    }

    return NULL;
}
