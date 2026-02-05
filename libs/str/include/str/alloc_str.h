#pragma once

#include <stddef.h>

#include "arena/arena.h"
#include "str/ref.h"

/// An owned string
typedef struct Str Str;

/// Creates an empty string with a capacity of `capacity`. You must call
/// `str_free` if `arena` is NULL.
///
/// The capacity is the length of string which can be held without reallocating
/// memory.
Str* str_new(size_t capacity, Arena* arena);

/// Frees a `Str` if is isn't arena allocated and sets it to NULL. If the string
/// was allocated on an arena, this will set the string to NULL, but no memory
/// will be freed.
void str_free(Str** str_ptr);

/// Creates an owned copy of `cstr`, which must be a null-terminated string.
Str* str_from_cstr(const char* cstr, Arena* arena);

/// Creates an owned copy of a `StrRef`. You must call `str_free` if `arena` is
/// NULL.
Str* str_copy_ref(StrRef ref, Arena* arena);

/// Creates a copy of a `Str`. The allocation method will be the same as the
/// original.
Str* str_clone(const Str* to_copy);

/// Gets a reference to this string. This string will be immutable after this.
/// Present and future clones will be mutable.
StrRef str_get_ref(Str* to_ref);

/// Gets the inner cstr
const char* str_get_cstr(const Str* str);

/// Gets the length of the string
size_t str_len(const Str* str);

/// Creates a new formatted string. You must call `str_free` if `arena` is NULL.
Str* str_new_fmt(Arena* arena, const char* fmt, ...);

/// Appends `to_append` to the string
void str_append(Str* str, const char* to_append);

/// Appends a formatted string to the string
void str_append_fmt(Str* str, const char* fmt, ...);
