/*
 * Bolt Test Suite - File Cache Tests
 * 
 * Tests for file cache operations.
 */

#include "minunit.h"
#include "../include/file_cache.h"
#include "../include/bolt.h"
#include <string.h>

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
 * Cache Add Tests
 *============================================================================*/

MU_TEST(test_cache_add_entry) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    mu_assert_not_null(cache);
    
    const char* path = "/test/file.txt";
    const char* content = "Hello, World!";
    const char* headers = "Content-Type: text/plain\r\n";
    
    bool result = bolt_file_cache_add(cache, path, content, strlen(content), 
                                       headers, strlen(headers));
    mu_assert_true(result);
    
    bolt_file_cache_destroy(cache);
    return NULL;
}

MU_TEST(test_cache_add_multiple) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    mu_assert_not_null(cache);
    
    const char* content = "Test content";
    const char* headers = "Content-Type: text/plain\r\n";
    
    mu_assert_true(bolt_file_cache_add(cache, "/file1.txt", content, strlen(content), headers, strlen(headers)));
    mu_assert_true(bolt_file_cache_add(cache, "/file2.txt", content, strlen(content), headers, strlen(headers)));
    mu_assert_true(bolt_file_cache_add(cache, "/file3.txt", content, strlen(content), headers, strlen(headers)));
    
    bolt_file_cache_destroy(cache);
    return NULL;
}

MU_TEST(test_cache_add_null_path) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    mu_assert_not_null(cache);
    
    bool result = bolt_file_cache_add(cache, NULL, "content", 7, "headers", 7);
    mu_assert_false(result);
    
    bolt_file_cache_destroy(cache);
    return NULL;
}

MU_TEST(test_cache_add_null_cache) {
    bool result = bolt_file_cache_add(NULL, "/path", "content", 7, "headers", 7);
    mu_assert_false(result);
    return NULL;
}

/*============================================================================
 * Cache Get Tests
 *============================================================================*/

MU_TEST(test_cache_get_existing) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    mu_assert_not_null(cache);
    
    const char* path = "/test/file.txt";
    const char* content = "Hello, World!";
    const char* headers = "Content-Type: text/plain\r\n";
    
    bolt_file_cache_add(cache, path, content, strlen(content), headers, strlen(headers));
    
    /* Get the cached entry */
    const char* cached_content;
    size_t cached_size;
    const char* cached_headers;
    size_t cached_headers_size;
    
    bool found = bolt_file_cache_get(cache, path, &cached_content, &cached_size,
                                     &cached_headers, &cached_headers_size);
    
    mu_assert_true(found);
    mu_assert_size_eq(strlen(content), cached_size);
    mu_check(memcmp(cached_content, content, cached_size) == 0);
    
    bolt_file_cache_destroy(cache);
    return NULL;
}

MU_TEST(test_cache_get_nonexistent) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    mu_assert_not_null(cache);
    
    const char* cached_content;
    size_t cached_size;
    const char* cached_headers;
    size_t cached_headers_size;
    
    bool found = bolt_file_cache_get(cache, "/nonexistent.txt", &cached_content, &cached_size,
                                     &cached_headers, &cached_headers_size);
    
    mu_assert_false(found);
    
    bolt_file_cache_destroy(cache);
    return NULL;
}

MU_TEST(test_cache_get_null_path) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    mu_assert_not_null(cache);
    
    const char* cached_content;
    size_t cached_size;
    const char* cached_headers;
    size_t cached_headers_size;
    
    bool found = bolt_file_cache_get(cache, NULL, &cached_content, &cached_size,
                                     &cached_headers, &cached_headers_size);
    
    mu_assert_false(found);
    
    bolt_file_cache_destroy(cache);
    return NULL;
}

/*============================================================================
 * Cache Remove Tests
 *============================================================================*/

MU_TEST(test_cache_remove_existing) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    mu_assert_not_null(cache);
    
    const char* path = "/test/file.txt";
    bolt_file_cache_add(cache, path, "content", 7, "headers", 7);
    
    /* Remove and verify it's gone */
    bolt_file_cache_remove(cache, path);
    
    const char* cached_content;
    size_t cached_size;
    const char* cached_headers;
    size_t cached_headers_size;
    
    bool found = bolt_file_cache_get(cache, path, &cached_content, &cached_size,
                                     &cached_headers, &cached_headers_size);
    
    mu_assert_false(found);
    
    bolt_file_cache_destroy(cache);
    return NULL;
}

MU_TEST(test_cache_remove_nonexistent) {
    BoltFileCache* cache = bolt_file_cache_create(100, 1024 * 1024);
    mu_assert_not_null(cache);
    
    /* Should not crash when removing nonexistent entry */
    bolt_file_cache_remove(cache, "/nonexistent.txt");
    
    bolt_file_cache_destroy(cache);
    return NULL;
}

/*============================================================================
 * Cache Capacity Tests
 *============================================================================*/

MU_TEST(test_cache_entry_limit) {
    /* Create cache with only 5 entries max */
    BoltFileCache* cache = bolt_file_cache_create(5, 1024 * 1024);
    mu_assert_not_null(cache);
    
    char path[64];
    const char* content = "Test content";
    const char* headers = "Headers\r\n";
    
    /* Add more entries than capacity */
    for (int i = 0; i < 10; i++) {
        snprintf(path, sizeof(path), "/file%d.txt", i);
        bolt_file_cache_add(cache, path, content, strlen(content), headers, strlen(headers));
    }
    
    /* Cache should still work (older entries may be evicted) */
    
    bolt_file_cache_destroy(cache);
    return NULL;
}

MU_TEST(test_cache_size_limit) {
    /* Create cache with small total size */
    BoltFileCache* cache = bolt_file_cache_create(100, 100);  /* 100 bytes max */
    mu_assert_not_null(cache);
    
    const char* large_content = "This is a longer piece of content that might exceed cache limits.";
    const char* headers = "Headers\r\n";
    
    /* Try to add content that might exceed limit */
    bolt_file_cache_add(cache, "/large.txt", large_content, strlen(large_content), 
                        headers, strlen(headers));
    
    /* Cache should handle this gracefully */
    
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
    
    /* Add */
    MU_RUN_TEST(test_cache_add_entry);
    MU_RUN_TEST(test_cache_add_multiple);
    MU_RUN_TEST(test_cache_add_null_path);
    MU_RUN_TEST(test_cache_add_null_cache);
    
    /* Get */
    MU_RUN_TEST(test_cache_get_existing);
    MU_RUN_TEST(test_cache_get_nonexistent);
    MU_RUN_TEST(test_cache_get_null_path);
    
    /* Remove */
    MU_RUN_TEST(test_cache_remove_existing);
    MU_RUN_TEST(test_cache_remove_nonexistent);
    
    /* Capacity */
    MU_RUN_TEST(test_cache_entry_limit);
    MU_RUN_TEST(test_cache_size_limit);
}

