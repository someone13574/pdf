#include "color/icc_cache.h"

#include <string.h>

#include "color/icc.h"
#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/ctx.h"

#define DVEC_NAME IccProfileCacheVec
#define DVEC_LOWERCASE_NAME icc_profile_cache_vec
#define DVEC_TYPE IccProfileCacheEntry
#include "arena/dvec_impl.h"

IccProfileCache icc_profile_cache_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    return (IccProfileCache) {.arena = arena,
                              .entries = icc_profile_cache_vec_new(arena)};
}

Error* icc_profile_cache_get(
    IccProfileCache* cache,
    const char* path,
    IccProfile** out
) {
    RELEASE_ASSERT(cache);
    RELEASE_ASSERT(path);
    RELEASE_ASSERT(out);

    for (size_t idx = 0; idx < icc_profile_cache_vec_len(cache->entries);
         idx++) {
        IccProfileCacheEntry* entry = NULL;
        RELEASE_ASSERT(
            icc_profile_cache_vec_get_ptr(cache->entries, idx, &entry)
        );

        if (strcmp(entry->path, path) == 0) {
            *out = &entry->profile;
            return NULL;
        }
    }

    IccProfileCacheEntry* entry = icc_profile_cache_vec_push(
        cache->entries,
        (IccProfileCacheEntry) {.path = path}
    );

    ParseCtx ctx = parse_ctx_from_file(cache->arena, path);
    TRY(icc_parse_profile(ctx, cache->arena, &entry->profile));
    *out = &entry->profile;

    return NULL;
}
