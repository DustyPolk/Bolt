#include "../include/memory_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Create a memory pool with per-arena allocation.
 */
BoltMemoryPool* bolt_pool_create(int num_arenas, size_t block_size) {
    BoltMemoryPool* pool = (BoltMemoryPool*)malloc(sizeof(BoltMemoryPool));
    if (!pool) {
        return NULL;
    }
    
    pool->num_arenas = num_arenas;
    pool->block_size = block_size;
    pool->total_allocations = 0;
    pool->total_frees = 0;
    pool->bytes_allocated = 0;
    
    /* Allocate arenas */
    pool->arenas = (BoltArena*)calloc(num_arenas, sizeof(BoltArena));
    if (!pool->arenas) {
        free(pool);
        return NULL;
    }
    
    /* Initialize each arena */
    for (int i = 0; i < num_arenas; i++) {
        pool->arenas[i].free_list = NULL;
        pool->arenas[i].large_blocks = NULL;
        pool->arenas[i].total_allocated = 0;
        pool->arenas[i].total_freed = 0;
        InitializeCriticalSection(&pool->arenas[i].lock);
        
        /* Pre-allocate some blocks */
        for (int j = 0; j < BOLT_POOL_INITIAL_BLOCKS / num_arenas; j++) {
            size_t alloc_size = sizeof(BoltMemBlock) + block_size;
            BoltMemBlock* block = (BoltMemBlock*)_aligned_malloc(alloc_size, BOLT_CACHE_LINE_SIZE);
            if (block) {
                block->size = block_size;
                block->next = pool->arenas[i].free_list;
                pool->arenas[i].free_list = block;
            }
        }
    }
    
    return pool;
}

/*
 * Destroy the memory pool.
 */
void bolt_pool_destroy(BoltMemoryPool* pool) {
    if (!pool) return;
    
    for (int i = 0; i < pool->num_arenas; i++) {
        /* Free all blocks in free list */
        BoltMemBlock* block = pool->arenas[i].free_list;
        while (block) {
            BoltMemBlock* next = block->next;
            _aligned_free(block);
            block = next;
        }
        
        /* Free large blocks */
        block = pool->arenas[i].large_blocks;
        while (block) {
            BoltMemBlock* next = block->next;
            _aligned_free(block);
            block = next;
        }
        
        DeleteCriticalSection(&pool->arenas[i].lock);
    }
    
    free(pool->arenas);
    free(pool);
}

/*
 * Allocate from the pool.
 */
void* bolt_pool_alloc(BoltMemoryPool* pool, int arena_id, size_t size) {
    if (!pool || arena_id < 0 || arena_id >= pool->num_arenas) {
        return malloc(size);  /* Fallback */
    }
    
    BoltArena* arena = &pool->arenas[arena_id];
    
    /* For standard-sized blocks, try free list first */
    if (size <= pool->block_size) {
        /* Try lock-free first read */
        if (arena->free_list) {
            EnterCriticalSection(&arena->lock);
            if (arena->free_list) {
                BoltMemBlock* block = arena->free_list;
                arena->free_list = block->next;
                arena->total_allocated += pool->block_size;
                LeaveCriticalSection(&arena->lock);
                
                InterlockedIncrement64(&pool->total_allocations);
                InterlockedAdd64(&pool->bytes_allocated, (LONG64)pool->block_size);
                
                return block->data;
            }
            LeaveCriticalSection(&arena->lock);
        }
        
        /* Allocate new block */
        size_t alloc_size = sizeof(BoltMemBlock) + pool->block_size;
        BoltMemBlock* block = (BoltMemBlock*)_aligned_malloc(alloc_size, BOLT_CACHE_LINE_SIZE);
        if (block) {
            block->size = pool->block_size;
            block->next = NULL;
            
            InterlockedIncrement64(&pool->total_allocations);
            InterlockedAdd64(&pool->bytes_allocated, (LONG64)pool->block_size);
            
            return block->data;
        }
        return NULL;
    }
    
    /* Large allocation */
    size_t alloc_size = sizeof(BoltMemBlock) + size;
    BoltMemBlock* block = (BoltMemBlock*)_aligned_malloc(alloc_size, BOLT_CACHE_LINE_SIZE);
    if (block) {
        block->size = size;
        block->next = NULL;
        
        /* Track large blocks for cleanup */
        EnterCriticalSection(&arena->lock);
        block->next = arena->large_blocks;
        arena->large_blocks = block;
        LeaveCriticalSection(&arena->lock);
        
        InterlockedIncrement64(&pool->total_allocations);
        InterlockedAdd64(&pool->bytes_allocated, (LONG64)size);
        
        return block->data;
    }
    
    return NULL;
}

/*
 * Free memory back to pool.
 */
void bolt_pool_free(BoltMemoryPool* pool, int arena_id, void* ptr) {
    if (!pool || !ptr) return;
    
    /* Get block header */
    BoltMemBlock* block = (BoltMemBlock*)((char*)ptr - offsetof(BoltMemBlock, data));
    
    if (arena_id < 0 || arena_id >= pool->num_arenas) {
        _aligned_free(block);
        return;
    }
    
    BoltArena* arena = &pool->arenas[arena_id];
    
    /* Standard block goes back to free list */
    if (block->size == pool->block_size) {
        EnterCriticalSection(&arena->lock);
        block->next = arena->free_list;
        arena->free_list = block;
        arena->total_freed += pool->block_size;
        LeaveCriticalSection(&arena->lock);
        
        InterlockedIncrement64(&pool->total_frees);
    } else {
        /* Large block - remove from tracking and free */
        EnterCriticalSection(&arena->lock);
        BoltMemBlock** pp = &arena->large_blocks;
        while (*pp) {
            if (*pp == block) {
                *pp = block->next;
                break;
            }
            pp = &(*pp)->next;
        }
        LeaveCriticalSection(&arena->lock);
        
        InterlockedIncrement64(&pool->total_frees);
        _aligned_free(block);
    }
}

/*
 * Get a pre-sized buffer (optimized path).
 */
void* bolt_pool_get_buffer(BoltMemoryPool* pool, int arena_id) {
    return bolt_pool_alloc(pool, arena_id, pool->block_size);
}

/*
 * Return a buffer to the pool.
 */
void bolt_pool_return_buffer(BoltMemoryPool* pool, int arena_id, void* buffer) {
    bolt_pool_free(pool, arena_id, buffer);
}

/*
 * Get pool statistics.
 */
void bolt_pool_stats(BoltMemoryPool* pool, size_t* total_alloc, size_t* total_free) {
    if (total_alloc) *total_alloc = (size_t)pool->total_allocations;
    if (total_free) *total_free = (size_t)pool->total_frees;
}

