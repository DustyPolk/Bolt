/*
 * Bolt Test Suite - File Cache Tests
 * 
 * Tests for file cache operations.
 */

#include "minunit.h"
#include "../include/file_cache.h"
#include "../include/bolt.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

/*============================================================================
 * Helpers
 *============================================================================*/

static void create_temp_file(const char* filename, const char* content) {
    FILE* f = fopen(filename, "wb");
    if (f) {
        fwrite(content, 1, strlen(content), f);
        fclose(f);
    }
}

static void delete_temp_file(const char* filename) {
    remove(filename);
}

/*============================================================================
 * Cache Creation/Destruction Tests
 *============================================================================*/

MU_TEST(test_cache_create) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    mu_assert_not_null(cache);
    bolt_file_cache_destroy(cache);
    return NULL;
}

MU_TEST(test_cache_create_small) {
    BoltFileCache* cache = bolt_file_cache_create(10, 4096);
    mu_assert_not_null(cache);
    bolt_file_cache_destroy(cache);
    return NULL;
}

MU_TEST(test_cache_destroy_null) {
    /* Should be safe to destroy NULL */
    bolt_file_cache_destroy(NULL);
    return NULL;
}

/*============================================================================
 * Cache Get Tests
 *============================================================================*/

MU_TEST(test_cache_get_existing) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    mu_assert_not_null(cache);
    
    const char* filename = "test_cache_file.txt";
    const char* content = "Hello, World!";
    create_temp_file(filename, content);
    
    struct stat st;
    mu_check(stat(filename, &st) == 0);
    
    BoltCachedResponse out;
    bool found = bolt_file_cache_get(cache, filename, "text/plain", st.st_mtime, st.st_size, &out);
    
    mu_assert_true(found);
    mu_assert_size_eq(strlen(content), out.body_len);
    mu_check(memcmp(out.body, content, out.body_len) == 0);
    
    delete_temp_file(filename);
    bolt_file_cache_destroy(cache);
    return NULL;
}

MU_TEST(test_cache_get_miss) {
    /* Test getting a file that exists but is not in cache yet (should load it) */
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    
    const char* filename = "test_cache_miss.txt";
    const char* content = "Cache Miss Content";
    create_temp_file(filename, content);
    
    struct stat st;
    stat(filename, &st);
    
    BoltCachedResponse out;
    /* First call loads it */
    bool found = bolt_file_cache_get(cache, filename, "text/plain", st.st_mtime, st.st_size, &out);
    mu_assert_true(found);
    mu_check(memcmp(out.body, content, out.body_len) == 0);
    
    /* Second call should hit cache (internal check, hard to verify from outside without mocking) */
    found = bolt_file_cache_get(cache, filename, "text/plain", st.st_mtime, st.st_size, &out);
    mu_assert_true(found);
    
    delete_temp_file(filename);
    bolt_file_cache_destroy(cache);
    return NULL;
}

MU_TEST(test_cache_get_nonexistent) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    
    BoltCachedResponse out;
    /* File doesn't exist, so stat would fail in real usage, 
       but if we pass dummy values, the cache loader will fail to read file */
    bool found = bolt_file_cache_get(cache, "nonexistent_file_999.txt", "text/plain", 12345, 100, &out);
    
    mu_assert_false(found);
    
    bolt_file_cache_destroy(cache);
    return NULL;
}

MU_TEST(test_cache_get_null_args) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    BoltCachedResponse out;
    
    /* NULL path */
    mu_assert_false(bolt_file_cache_get(cache, NULL, "type", 0, 0, &out));
    
    /* NULL cache */
    mu_assert_false(bolt_file_cache_get(NULL, "path", "type", 0, 0, &out));
    
    /* NULL out */
    mu_assert_false(bolt_file_cache_get(cache, "path", "type", 0, 0, NULL));
    
    bolt_file_cache_destroy(cache);
    return NULL;
}

/*============================================================================
 * Cache Update/Stale Tests
 *============================================================================*/

MU_TEST(test_cache_stale_update) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    
    const char* filename = "test_cache_stale.txt";
    create_temp_file(filename, "Version 1");
    
    struct stat st;
    stat(filename, &st);
    
    BoltCachedResponse out;
    bolt_file_cache_get(cache, filename, "text/plain", st.st_mtime, st.st_size, &out);
    mu_check(memcmp(out.body, "Version 1", 9) == 0);
    
    /* Update file */
    /* Wait a bit to ensure mtime changes if resolution is low, or just rely on size change if possible, 
       but here we change content. Windows mtime resolution is usually good. */
    /* Force a small sleep might be needed for low-res filesystems, but let's try just overwriting */
    /* Alternatively, we can fake the mtime passed to get() to simulate staleness */
    
    create_temp_file(filename, "Version 2 - Updated");
    stat(filename, &st);
    
    /* Call get with new mtime/size */
    bool found = bolt_file_cache_get(cache, filename, "text/plain", st.st_mtime, st.st_size, &out);
    
    mu_assert_true(found);
    mu_check(memcmp(out.body, "Version 2 - Updated", 19) == 0);
    
    delete_temp_file(filename);
    bolt_file_cache_destroy(cache);
    return NULL;
}

/*============================================================================
 * Test Suite Runner
 *============================================================================*/

void test_suite_cache(void) {
    /* Creation/destruction */
    MU_RUN_TEST(test_cache_create);
    MU_RUN_TEST(test_cache_create_small);
    MU_RUN_TEST(test_cache_destroy_null);
    
    /* Get operations */
    MU_RUN_TEST(test_cache_get_existing);
    MU_RUN_TEST(test_cache_get_miss);
    MU_RUN_TEST(test_cache_get_nonexistent);
    MU_RUN_TEST(test_cache_get_null_args);
    
    /* Updates */
    MU_RUN_TEST(test_cache_stale_update);
}
