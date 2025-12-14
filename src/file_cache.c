#include "../include/file_cache.h"
#include "../include/utils.h"
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool used;
    uint32_t hash;
    time_t mtime;
    size_t file_size;
    size_t total_bytes; /* headers+body */
    ULONGLONG last_used;
    char path[BOLT_MAX_PATH_LENGTH];
    char* headers;
    size_t headers_len;
    char* body;
    size_t body_len;
} CacheEntry;

struct BoltFileCache {
    SRWLOCK lock;
    CacheEntry* entries;
    size_t capacity;
    size_t max_total_bytes;
    size_t total_bytes;
};

static uint32_t fnv1a32(const char* s) {
    uint32_t h = 2166136261u;
    while (*s) {
        h ^= (uint8_t)(*s++);
        h *= 16777619u;
    }
    return h ? h : 1u;
}

static size_t build_200_headers(char* out, size_t out_sz,
                               const char* content_type,
                               size_t content_length,
                               time_t mtime) {
    char last_modified[64];
    char etag[64];
    utils_format_http_date(mtime, last_modified, sizeof(last_modified));
    snprintf(etag, sizeof(etag), "\"%zx-%lx\"", content_length, (unsigned long)mtime);

    return (size_t)snprintf(out, out_sz,
        "HTTP/1.1 200 OK\r\n"
        "Server: " BOLT_SERVER_NAME "\r\n"
        "Connection: keep-alive\r\n"
        "Keep-Alive: timeout=60, max=1000\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: public, max-age=3600\r\n"
        "ETag: %s\r\n"
        "Last-Modified: %s\r\n"
        "X-Frame-Options: DENY\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "\r\n",
        content_type ? content_type : "application/octet-stream",
        content_length,
        etag,
        last_modified
    );
}

static bool read_entire_file(const char* filepath, size_t size, char* out_buf) {
    HANDLE file = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;

    DWORD read_bytes = 0;
    bool ok = ReadFile(file, out_buf, (DWORD)size, &read_bytes, NULL) && read_bytes == (DWORD)size;
    CloseHandle(file);
    return ok;
}

BoltFileCache* bolt_file_cache_create(size_t capacity, size_t max_total_bytes) {
    BoltFileCache* c = (BoltFileCache*)calloc(1, sizeof(BoltFileCache));
    if (!c) return NULL;
    InitializeSRWLock(&c->lock);
    c->capacity = capacity;
    c->max_total_bytes = max_total_bytes;
    c->entries = (CacheEntry*)calloc(capacity, sizeof(CacheEntry));
    if (!c->entries) {
        free(c);
        return NULL;
    }
    return c;
}

static void free_entry(BoltFileCache* cache, CacheEntry* e) {
    if (!e->used) return;
    if (e->headers) free(e->headers);
    if (e->body) free(e->body);
    if (cache->total_bytes >= e->total_bytes) cache->total_bytes -= e->total_bytes;
    memset(e, 0, sizeof(*e));
}

void bolt_file_cache_destroy(BoltFileCache* cache) {
    if (!cache) return;
    AcquireSRWLockExclusive(&cache->lock);
    for (size_t i = 0; i < cache->capacity; i++) {
        free_entry(cache, &cache->entries[i]);
    }
    ReleaseSRWLockExclusive(&cache->lock);
    free(cache->entries);
    free(cache);
}

static CacheEntry* find_slot(BoltFileCache* cache, uint32_t hash, const char* filepath) {
    size_t idx = (size_t)(hash % cache->capacity);
    for (size_t probe = 0; probe < cache->capacity; probe++) {
        CacheEntry* e = &cache->entries[idx];
        if (!e->used) return e;
        if (e->hash == hash && strcmp(e->path, filepath) == 0) return e;
        idx = (idx + 1) % cache->capacity;
    }
    return NULL;
}

static CacheEntry* find_lru(BoltFileCache* cache) {
    CacheEntry* best = NULL;
    for (size_t i = 0; i < cache->capacity; i++) {
        CacheEntry* e = &cache->entries[i];
        if (!e->used) return e;
        if (!best || e->last_used < best->last_used) best = e;
    }
    return best;
}

bool bolt_file_cache_get(BoltFileCache* cache,
                         const char* filepath,
                         const char* content_type,
                         time_t mtime,
                         size_t file_size,
                         BoltCachedResponse* out) {
    if (!cache || !filepath || !out) return false;

    /* Hard limit: body must fit plus headers into the connection send buffer */
    if (file_size == 0 || file_size > (size_t)(BOLT_FILE_CACHE_MAX_ENTRY_SIZE - 1024)) {
        return false;
    }

    uint32_t h = fnv1a32(filepath);

    /* Fast path: shared lock lookup */
    AcquireSRWLockShared(&cache->lock);
    size_t idx = (size_t)(h % cache->capacity);
    for (size_t probe = 0; probe < cache->capacity; probe++) {
        CacheEntry* e = &cache->entries[idx];
        if (!e->used) break;
        if (e->hash == h && strcmp(e->path, filepath) == 0) {
            if (e->mtime == mtime && e->file_size == file_size) {
                e->last_used = GetTickCount64();
                out->headers = e->headers;
                out->headers_len = e->headers_len;
                out->body = e->body;
                out->body_len = e->body_len;
                ReleaseSRWLockShared(&cache->lock);
                return true;
            }
            break;
        }
        idx = (idx + 1) % cache->capacity;
    }
    ReleaseSRWLockShared(&cache->lock);

    /* Slow path: exclusive lock load/refresh */
    AcquireSRWLockExclusive(&cache->lock);

    CacheEntry* e = find_slot(cache, h, filepath);
    if (!e) {
        ReleaseSRWLockExclusive(&cache->lock);
        return false;
    }

    /* If occupied by same path but stale, free it first */
    if (e->used && e->hash == h && strcmp(e->path, filepath) == 0) {
        free_entry(cache, e);
    } else if (e->used) {
        /* Collision slot filled by other key; evict LRU instead */
        CacheEntry* victim = find_lru(cache);
        if (!victim) {
            ReleaseSRWLockExclusive(&cache->lock);
            return false;
        }
        free_entry(cache, victim);
        e = victim;
    }

    /* Build headers */
    char header_tmp[1024];
    size_t hdr_len = build_200_headers(header_tmp, sizeof(header_tmp), content_type, file_size, mtime);
    if (hdr_len == 0 || hdr_len >= sizeof(header_tmp)) {
        ReleaseSRWLockExclusive(&cache->lock);
        return false;
    }

    size_t total = hdr_len + file_size;
    if (total > BOLT_FILE_CACHE_MAX_ENTRY_SIZE) {
        ReleaseSRWLockExclusive(&cache->lock);
        return false;
    }

    /* Enforce total cache cap */
    while (cache->total_bytes + total > cache->max_total_bytes) {
        CacheEntry* victim = find_lru(cache);
        if (!victim || (!victim->used && victim == e)) break;
        if (victim->used) free_entry(cache, victim);
        else break;
    }

    /* Allocate and read file */
    char* body = (char*)malloc(file_size);
    if (!body) {
        ReleaseSRWLockExclusive(&cache->lock);
        return false;
    }
    if (!read_entire_file(filepath, file_size, body)) {
        free(body);
        ReleaseSRWLockExclusive(&cache->lock);
        return false;
    }

    char* headers = (char*)malloc(hdr_len);
    if (!headers) {
        free(body);
        ReleaseSRWLockExclusive(&cache->lock);
        return false;
    }
    memcpy(headers, header_tmp, hdr_len);

    /* Commit entry */
    memset(e, 0, sizeof(*e));
    e->used = true;
    e->hash = h;
    e->mtime = mtime;
    e->file_size = file_size;
    e->headers = headers;
    e->headers_len = hdr_len;
    e->body = body;
    e->body_len = file_size;
    e->total_bytes = total;
    e->last_used = GetTickCount64();
    strncpy(e->path, filepath, sizeof(e->path) - 1);

    cache->total_bytes += total;

    out->headers = e->headers;
    out->headers_len = e->headers_len;
    out->body = e->body;
    out->body_len = e->body_len;

    ReleaseSRWLockExclusive(&cache->lock);
    return true;
}


