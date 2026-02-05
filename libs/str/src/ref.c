#include "str/ref.h"

#include <string.h>

#include "logger/log.h"

StrRef str_ref_from_cstr(const char* cstr) {
    return (StrRef) {.data = cstr, .len = strlen(cstr), .terminated = true};
}

StrRef str_ref_from_buffer_const(const char* buffer, size_t len) {
    return (StrRef) {.data = buffer, .len = len, .terminated = false};
}

StrRef str_ref_slice(StrRef str, size_t start, size_t end) {
    RELEASE_ASSERT(start <= end);
    RELEASE_ASSERT(end <= str.len);

    return (StrRef) {.data = str.data + start,
                     .len = end - start,
                     .terminated = str.terminated && end == str.len};
}

StrRef str_ref_slice_remaining(StrRef str, size_t start) {
    RELEASE_ASSERT(start <= str.len);

    return (StrRef) {.data = str.data + start,
                     .len = str.len - start,
                     .terminated = str.terminated};
}
