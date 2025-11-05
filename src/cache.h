#ifndef CACHE_H
#define CACHE_H

#include <sys/stat.h>

#include "ds.h"

s8 get_or_make_cache_dir(Arena *perm, s8 name);

#endif // CACHE_H

#ifdef CACHE_IMPL
#ifndef CACHE_IMPL_GUARD
#define CACHE_IMPL_GUARD

#define DS_IMPL
#include "ds.h"

#ifdef __APPLE__
#define _CACHE_DIR_123 "/Library/Application Support"
#else
#define _CACHE_DIR_123 "/.cache"
#endif

s8 get_or_make_cache_dir(Arena *perm, s8 name) {
    s8 cache;

    do {
        cache.buf = (u8 *) getenv("XDG_CACHE_HOME");
        if (cache.buf != NULL) {
            cache.len = strlen((char *) cache.buf);
            cache = s8_copy(perm, cache);
            break;
        }

        cache.buf = (u8 *) getenv("HOME");
        if (cache.buf != NULL) {
            cache.len = strlen((char *) cache.buf);
            cache = s8_newcat(perm, cache, s8(_CACHE_DIR_123));
            break;
        }
        return (s8) {0};
    } while (0);

    s8_modcat(perm, &cache, s8("/"));
    s8_modcat(perm, &cache, name);
    s8_modcat(perm, &cache, s8("\0"));

#ifdef _WIN32
    int r = mkdir((char *) cache.buf);
#else
    int r = mkdir((char *) cache.buf, 0700);
#endif

    if (!r) {
        printf("No cache directory. Making one at %s.\n", cache.buf);
    } else if (errno != EEXIST) {
        warning(
            "Could not get or make cache directory: %s. Disabling cache support.",
            strerror(errno)
        );
        return (s8) {0};
    }

    cache.len -= 1; // remove null terminator
    perm->len -= 1;

    return cache;
}

#endif // CACHE_IMPL_GUARD
#endif // CACHE_IMPL
