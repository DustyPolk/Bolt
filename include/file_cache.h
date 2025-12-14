#ifndef FILE_CACHE_H
#define FILE_CACHE_H

#include "bolt.h"
#include <stddef.h>
#include <time.h>

/*
 * Small-file cache for mixed-site performance.
 * Caches small assets in memory to avoid disk + open/close overhead.
 */

typedef struct BoltFileCache BoltFileCache;

typedef struct {
    const char* headers;
    size_t headers_len;
    const char* body;
    size_t body_len;
} BoltCachedResponse;

BoltFileCache* bolt_file_cache_create(size_t capacity, size_t max_total_bytes);
void bolt_file_cache_destroy(BoltFileCache* cache);

/*
 * Lookup a cached response for a given filepath. If not present or stale, may load it.
 * Returns true if a cached (or newly cached) response is available.
 *
 * Notes:
 * - Only caches files <= BOLT_FILE_CACHE_MAX_ENTRY_SIZE (including headers).
 * - Uses file mtime+size to validate staleness.
 */
bool bolt_file_cache_get(BoltFileCache* cache,
                         const char* filepath,
                         const char* content_type,
                         time_t mtime,
                         size_t file_size,
                         BoltCachedResponse* out);

#endif /* FILE_CACHE_H */


