#pragma once

#include "arena/arena.h"

/// An arena-backed string
typedef struct ArenaString ArenaString;

/// Creates a new arena-backed string with a given capacity
ArenaString* arena_string_new(Arena* arena, size_t capacity);

/// Creates a new arena-backed string by preforming a format.
ArenaString* arena_string_new_fmt(Arena* arena, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

/// Gets the underlying null-terminated string
const char* arena_string_buffer(ArenaString* string);

/// Gets the length of the string, excluding the null-terminator
size_t arena_string_len(ArenaString* string);

/// Appends a string to the end of this string
void arena_string_append(ArenaString* string, const char* to_append);
