/*
 * Bolt Test Suite - Memory Pool Tests
 * 
 * Tests for memory pool allocation and management.
 */

#include "minunit.h"
#include "../include/memory_pool.h"
#include "../include/bolt.h"
#include <string.h>

/*============================================================================
 * Pool Creation/Destruction Tests
 *============================================================================*/

MU_TEST(test_pool_create) {
    BoltMemoryPool* pool = bolt_pool_create(4, 4096);
    mu_assert_not_null(pool);
    bolt_pool_destroy(pool);
    return NULL;
}

MU_TEST(test_pool_create_single_arena) {
    BoltMemoryPool* pool = bolt_pool_create(1, 1024);
    mu_assert_not_null(pool);
    bolt_pool_destroy(pool);
    return NULL;
}

MU_TEST(test_pool_create_many_arenas) {
    BoltMemoryPool* pool = bolt_pool_create(16, 4096);
    mu_assert_not_null(pool);
    bolt_pool_destroy(pool);
    return NULL;
}

MU_TEST(test_pool_destroy_null) {
    /* Should be safe to call with NULL */
    bolt_pool_destroy(NULL);
    return NULL;
}

/*============================================================================
 * Allocation Tests
 *============================================================================*/

MU_TEST(test_pool_alloc_small) {
    BoltMemoryPool* pool = bolt_pool_create(4, 4096);
    mu_assert_not_null(pool);
    
    void* ptr = bolt_pool_alloc(pool, 0, 64);
    mu_assert_not_null(ptr);
    
    /* Should be able to write to allocated memory */
    memset(ptr, 0xAA, 64);
    
    bolt_pool_free(pool, 0, ptr);
    bolt_pool_destroy(pool);
    return NULL;
}

MU_TEST(test_pool_alloc_medium) {
    BoltMemoryPool* pool = bolt_pool_create(4, 4096);
    mu_assert_not_null(pool);
    
    void* ptr = bolt_pool_alloc(pool, 0, 1024);
    mu_assert_not_null(ptr);
    
    memset(ptr, 0xBB, 1024);
    
    bolt_pool_free(pool, 0, ptr);
    bolt_pool_destroy(pool);
    return NULL;
}

MU_TEST(test_pool_alloc_multiple) {
    BoltMemoryPool* pool = bolt_pool_create(4, 8192);
    mu_assert_not_null(pool);
    
    void* ptr1 = bolt_pool_alloc(pool, 0, 256);
    void* ptr2 = bolt_pool_alloc(pool, 0, 256);
    void* ptr3 = bolt_pool_alloc(pool, 0, 256);
    
    mu_assert_not_null(ptr1);
    mu_assert_not_null(ptr2);
    mu_assert_not_null(ptr3);
    
    /* Pointers should be different */
    mu_check(ptr1 != ptr2);
    mu_check(ptr2 != ptr3);
    mu_check(ptr1 != ptr3);
    
    bolt_pool_free(pool, 0, ptr3);
    bolt_pool_free(pool, 0, ptr2);
    bolt_pool_free(pool, 0, ptr1);
    bolt_pool_destroy(pool);
    return NULL;
}

MU_TEST(test_pool_alloc_different_arenas) {
    BoltMemoryPool* pool = bolt_pool_create(4, 4096);
    mu_assert_not_null(pool);
    
    void* ptr0 = bolt_pool_alloc(pool, 0, 256);
    void* ptr1 = bolt_pool_alloc(pool, 1, 256);
    void* ptr2 = bolt_pool_alloc(pool, 2, 256);
    void* ptr3 = bolt_pool_alloc(pool, 3, 256);
    
    mu_assert_not_null(ptr0);
    mu_assert_not_null(ptr1);
    mu_assert_not_null(ptr2);
    mu_assert_not_null(ptr3);
    
    bolt_pool_free(pool, 0, ptr0);
    bolt_pool_free(pool, 1, ptr1);
    bolt_pool_free(pool, 2, ptr2);
    bolt_pool_free(pool, 3, ptr3);
    bolt_pool_destroy(pool);
    return NULL;
}

/*============================================================================
 * Free Tests
 *============================================================================*/

MU_TEST(test_pool_free_and_realloc) {
    BoltMemoryPool* pool = bolt_pool_create(4, 4096);
    mu_assert_not_null(pool);
    
    void* ptr1 = bolt_pool_alloc(pool, 0, 256);
    mu_assert_not_null(ptr1);
    
    bolt_pool_free(pool, 0, ptr1);
    
    /* Should be able to allocate again after free */
    void* ptr2 = bolt_pool_alloc(pool, 0, 256);
    mu_assert_not_null(ptr2);
    
    bolt_pool_free(pool, 0, ptr2);
    bolt_pool_destroy(pool);
    return NULL;
}

MU_TEST(test_pool_free_null) {
    BoltMemoryPool* pool = bolt_pool_create(4, 4096);
    mu_assert_not_null(pool);
    
    /* Should be safe to free NULL */
    bolt_pool_free(pool, 0, NULL);
    
    bolt_pool_destroy(pool);
    return NULL;
}

/*============================================================================
 * Boundary Tests
 *============================================================================*/

MU_TEST(test_pool_alloc_zero_size) {
    BoltMemoryPool* pool = bolt_pool_create(4, 4096);
    mu_assert_not_null(pool);
    
    /* Zero-size allocation behavior is implementation-defined */
    void* ptr = bolt_pool_alloc(pool, 0, 0);
    /* Either NULL or valid pointer is acceptable */
    
    if (ptr) {
        bolt_pool_free(pool, 0, ptr);
    }
    
    bolt_pool_destroy(pool);
    return NULL;
}

MU_TEST(test_pool_alloc_invalid_arena) {
    BoltMemoryPool* pool = bolt_pool_create(4, 4096);
    mu_assert_not_null(pool);
    
    /* Arena ID out of range should be handled */
    void* ptr = bolt_pool_alloc(pool, 999, 256);
    /* Should either return NULL or wrap around */
    
    if (ptr) {
        bolt_pool_free(pool, 999, ptr);
    }
    
    bolt_pool_destroy(pool);
    return NULL;
}

MU_TEST(test_pool_alloc_null_pool) {
    /* Should handle NULL pool gracefully */
    void* ptr = bolt_pool_alloc(NULL, 0, 256);
    mu_assert_null(ptr);
    return NULL;
}

/*============================================================================
 * Stress Tests
 *============================================================================*/

MU_TEST(test_pool_many_allocations) {
    BoltMemoryPool* pool = bolt_pool_create(4, 65536);
    mu_assert_not_null(pool);
    
    void* ptrs[100];
    
    /* Allocate many small blocks */
    for (int i = 0; i < 100; i++) {
        ptrs[i] = bolt_pool_alloc(pool, i % 4, 64);
        if (ptrs[i]) {
            memset(ptrs[i], i & 0xFF, 64);
        }
    }
    
    /* Free all */
    for (int i = 0; i < 100; i++) {
        if (ptrs[i]) {
            bolt_pool_free(pool, i % 4, ptrs[i]);
        }
    }
    
    bolt_pool_destroy(pool);
    return NULL;
}

/*============================================================================
 * Test Suite Runner
 *============================================================================*/

void test_suite_pool(void) {
    /* Creation/destruction */
    MU_RUN_TEST(test_pool_create);
    MU_RUN_TEST(test_pool_create_single_arena);
    MU_RUN_TEST(test_pool_create_many_arenas);
    MU_RUN_TEST(test_pool_destroy_null);
    
    /* Allocation */
    MU_RUN_TEST(test_pool_alloc_small);
    MU_RUN_TEST(test_pool_alloc_medium);
    MU_RUN_TEST(test_pool_alloc_multiple);
    MU_RUN_TEST(test_pool_alloc_different_arenas);
    
    /* Free */
    MU_RUN_TEST(test_pool_free_and_realloc);
    MU_RUN_TEST(test_pool_free_null);
    
    /* Boundary conditions */
    MU_RUN_TEST(test_pool_alloc_zero_size);
    MU_RUN_TEST(test_pool_alloc_invalid_arena);
    MU_RUN_TEST(test_pool_alloc_null_pool);
    
    /* Stress */
    MU_RUN_TEST(test_pool_many_allocations);
}

