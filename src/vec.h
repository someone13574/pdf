#pragma once

#include "arena.h"

typedef struct Vec Vec;

Vec* vec_new(Arena* arena);

void vec_push(Vec* vec, void* element);
void* vec_pop(Vec* vec);

void* vec_get(Vec* vec, size_t idx);
size_t vec_len(Vec*);
