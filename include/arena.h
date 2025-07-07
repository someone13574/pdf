#pragma once

#include <stddef.h>

typedef struct Arena Arena;

Arena* arena_new(size_t block_size);
void arena_free(Arena* arena);

void* arena_alloc(Arena* arena, size_t size);
void* arena_alloc_align(Arena* arena, size_t size, size_t align);

void arena_reset(Arena* arena);
