#include "adler32.h"

#include "logger/log.h"

Adler32Sum adler32_compute_checksum(const uint8_t* data, size_t data_len) {
    RELEASE_ASSERT(data);

    uint32_t s1 = 1;
    uint32_t s2 = 0;

    for (size_t idx = 0; idx < data_len; idx++) {
        s1 += data[idx];
        if (s1 >= 65521) {
            s1 -= 65521;
        }

        s2 += s1;
        if (s2 >= 65521) {
            s2 -= 65521;
        }
    }

    return (s2 << 16) | s1;
}

void adler32_swap_endianness(Adler32Sum* sum) {
    RELEASE_ASSERT(sum);

    *sum = ((*sum >> 24) & 0xff) | ((*sum << 8) & 0xff0000)
         | ((*sum >> 8) & 0xff00) | ((*sum << 24) & 0xff000000);
}
