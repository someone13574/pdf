#pragma once

#include <stdbool.h>
#include <stddef.h>

/// A reference to a string or string slice
typedef struct {
    const char* data;
    const size_t len;
    const bool terminated;
} StrRef;

/// Create a new const str from a null-terminated string. This will be valid for
/// the lifetime of `cstr`
StrRef str_ref_from_cstr(const char* cstr);

/// Create a new const str from unterminated data. This will be valid for the
/// lifetime of `buffer`.
StrRef str_ref_from_buffer_const(const char* buffer, size_t len);

/// Create a slice of an existing string, from `start` to `end` (end is
/// exclusive). This will panic if out of bounds.
StrRef str_ref_slice(StrRef str, size_t start, size_t end);

/// Create a slice of an existing string, from `start` to the end. This will
/// panic if out of bounds.
StrRef str_ref_slice_remaining(StrRef str, size_t start);
