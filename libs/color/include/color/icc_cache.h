#pragma once

#include "arena/arena.h"
#include "color/icc.h"

typedef struct {
    const char* path;
    IccProfile profile;
} IccProfileCacheEntry;

#define DVEC_NAME IccProfileCacheVec
#define DVEC_LOWERCASE_NAME icc_profile_cache_vec
#define DVEC_TYPE IccProfileCacheEntry
#include "arena/dvec_decl.h"

typedef struct {
    Arena* arena;
    IccProfileCacheVec* entries;
} IccProfileCache;

IccProfileCache icc_profile_cache_new(Arena* arena);
Error* icc_profile_cache_get(
    IccProfileCache* cache,
    const char* path,
    IccProfile** out
);
