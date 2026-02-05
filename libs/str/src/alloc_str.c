#include "str/alloc_str.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "str/ref.h"

struct Str {
    Arena* arena;
    size_t len;
    size_t capacity;
    char* data;
    bool mutable;
};

static void* any_alloc(Arena* arena, size_t size) {
    if (arena) {
        return arena_alloc(arena, size);
    } else {
        return malloc(size);
    }
}

Str* str_new(size_t capacity, Arena* arena) {
    Str* str = any_alloc(arena, sizeof(Str));
    str->arena = arena;
    str->len = 0;
    str->capacity = capacity;
    str->data =
        capacity == 0 ? NULL : any_alloc(arena, sizeof(char) * (capacity + 1));
    str->mutable = true;

    return str;
}

void str_free(Str** str_ptr) {
    RELEASE_ASSERT(str_ptr);
    RELEASE_ASSERT(*str_ptr);

    Str* str = *str_ptr;

    if (!str->arena) {
        if (str->data) {
            free(str->data);
        }
        free(str);
    }

    *str_ptr = NULL;
}

Str* str_from_cstr(const char* cstr, Arena* arena) {
    return str_copy_ref(str_ref_from_cstr(cstr), arena);
}

Str* str_copy_ref(StrRef ref, Arena* arena) {
    RELEASE_ASSERT(ref.data);

    Str* str = str_new(ref.len, arena);
    if (ref.len > 0) {
        memcpy(str->data, ref.data, ref.len);
        str->data[ref.len] = '\0';
    }
    str->len = ref.len;
    return str;
}

Str* str_clone(const Str* to_copy) {
    RELEASE_ASSERT(to_copy);

    Str* str = str_new(to_copy->capacity, to_copy->arena);
    if (to_copy->capacity > 0) {
        memcpy(str->data, to_copy->data, to_copy->len + 1);
    }
    str->len = to_copy->len;
    return str;
}

StrRef str_get_ref(Str* to_ref) {
    to_ref->mutable = false;
    return (StrRef) {.data = to_ref->data,
                     .len = to_ref->len,
                     .terminated = true};
}

const char* str_get_cstr(const Str* str) {
    RELEASE_ASSERT(str);
    return str->data;
}

size_t str_len(const Str* str) {
    RELEASE_ASSERT(str);
    return str->len;
}

static void* any_realloc(Str* str, size_t new_capacity) {
    if (str->arena) {
        char* new_data = arena_alloc(str->arena, new_capacity + 1);
        if (str->data) {
            memcpy(new_data, str->data, str->len + 1);
        }
        return new_data;
    } else {
        return realloc(str->data, new_capacity + 1);
    }
}

Str* str_new_fmt(Arena* arena, const char* fmt, ...) {
    RELEASE_ASSERT(fmt);

    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    Str* str = str_new((size_t)needed, arena);
    vsprintf(str->data, fmt, args);
    str->len = (size_t)needed;
    va_end(args);

    return str;
}

void str_append(Str* str, const char* to_append) {
    RELEASE_ASSERT(str);
    RELEASE_ASSERT(to_append);
    RELEASE_ASSERT(str->mutable);

    size_t append_len = strlen(to_append);
    size_t required_capacity = str->len + append_len;

    if (required_capacity > str->capacity) {
        str->capacity = required_capacity * 2;
        str->data = any_realloc(str, str->capacity);
    }

    strcpy(str->data + str->len, to_append);
    str->len += append_len;
}

void str_append_fmt(Str* str, const char* fmt, ...) {
    RELEASE_ASSERT(str);
    RELEASE_ASSERT(fmt);
    RELEASE_ASSERT(str->mutable);

    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    size_t required_capacity = str->len + (size_t)needed;

    if (required_capacity > str->capacity) {
        str->capacity = required_capacity * 2;
        str->data = any_realloc(str, str->capacity);
    }

    vsprintf(str->data + str->len, fmt, args);
    str->len += (size_t)needed;
    va_end(args);
}
