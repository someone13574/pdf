#pragma once

#include <stddef.h>

typedef struct Arena Arena;

/// Creates a new dynamically-allocated arena
Arena* arena_new(size_t block_size);

/// Creates a new arena within an existing buffer. Note that the whole buffer
/// size may not be usable.
Arena* arena_new_in_buffer(void* buffer, size_t buffer_len);

/// Frees the dynamically-allocated arena. If the arena was initialized in a
/// fixed buffer, this will panic.
void arena_free(Arena* arena);

/// Allocates a region of `size` bytes on the arena, with an alignment of
/// `max_align_t`.
void* arena_alloc(Arena* arena, size_t size);

/// Allocates a region of `size` bytes on the arena with a custom alignment.
void* arena_alloc_align(Arena* arena, size_t size, size_t align);

/// Resets the arena, invalidating everything previously allocated on it. Note
/// that this does not free any memory.
void arena_reset(Arena* arena);
