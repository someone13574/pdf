#include "arena/string.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"

struct ArenaString {
    Arena* arena;
    size_t capacity; /// capacity of buffer, including the null-terminator

    char* buffer;
};

ArenaString* arena_string_new(Arena* arena, size_t capacity) {
    RELEASE_ASSERT(arena);

    LOG_DIAG(
        INFO,
        STRING,
        "Creating new empty string with capacity %zu",
        capacity
    );

    ArenaString* string = arena_alloc(arena, sizeof(ArenaString));
    string->arena = arena;
    string->capacity = capacity + 1;
    string->buffer = arena_alloc(arena, capacity + 1);
    string->buffer[0] = '\0';

    return string;
}

ArenaString* arena_string_new_fmt(Arena* arena, const char* fmt, ...) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(fmt);

    LOG_DIAG(
        INFO,
        STRING,
        "Creating new formatted string with format `%s`",
        fmt
    );

    ArenaString* string = arena_alloc(arena, sizeof(ArenaString));

    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);

    LOG_DIAG(TRACE, STRING, "%d+1 bytes needed for formatted string", needed);

    va_end(args_copy);
    string->buffer = arena_alloc(arena, (size_t)needed + 1);
    vsprintf(string->buffer, fmt, args);
    va_end(args);

    return string;
}

const char* arena_string_buffer(const ArenaString* string) {
    RELEASE_ASSERT(string);

    return string->buffer;
}

size_t arena_string_len(const ArenaString* string) {
    RELEASE_ASSERT(string);

    return strlen(string->buffer);
}

void arena_string_append(ArenaString* string, const char* to_append) {
    RELEASE_ASSERT(string);
    RELEASE_ASSERT(to_append);

    LOG_DIAG(DEBUG, STRING, "Appending string to string");

    size_t curr_len = strlen(string->buffer);
    size_t append_len = strlen(to_append);
    size_t required_capacity = curr_len + append_len + 1;

    if (required_capacity > string->capacity) {
        LOG_DIAG(
            TRACE,
            STRING,
            "Allocating new buffer due to insufficient size"
        );

        const char* old_buffer = string->buffer;

        string->capacity = required_capacity * 2;
        string->buffer = arena_alloc(string->arena, string->capacity);

        strncpy(string->buffer, old_buffer, curr_len);
        strcpy(string->buffer + curr_len, to_append);
    } else {
        strcpy(string->buffer + curr_len, to_append);
    }
}
