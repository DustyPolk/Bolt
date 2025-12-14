#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include "bolt.h"
#include <stddef.h>

/*
 * Lock-free memory pool for high-performance allocation.
 * Uses per-thread arenas to avoid contention.
 */

/* Memory block header */
typedef struct BoltMemBlock {
    struct BoltMemBlock* next;
    size_t size;
    uint8_t data[];
} BoltMemBlock;

/* Per-thread arena */
typedef struct BoltArena {
    BoltMemBlock* free_list;
    BoltMemBlock* large_blocks;
    size_t total_allocated;
    size_t total_freed;
    CRITICAL_SECTION lock;  /* Only for cross-thread operations */
} BoltArena;

/* Global memory pool */
struct BoltMemoryPool {
    BoltArena* arenas;
    int num_arenas;
    size_t block_size;
    
    /* Statistics */
    volatile LONG64 total_allocations;
    volatile LONG64 total_frees;
    volatile LONG64 bytes_allocated;
};

/*
 * Initialize the memory pool.
 * num_arenas should match thread count for best performance.
 */
BoltMemoryPool* bolt_pool_create(int num_arenas, size_t block_size);

/*
 * Destroy the memory pool and free all memory.
 */
void bolt_pool_destroy(BoltMemoryPool* pool);

/*
 * Allocate memory from the pool.
 * arena_id should be the calling thread's ID for best performance.
 */
void* bolt_pool_alloc(BoltMemoryPool* pool, int arena_id, size_t size);

/*
 * Free memory back to the pool.
 */
void bolt_pool_free(BoltMemoryPool* pool, int arena_id, void* ptr);

/*
 * Get a pre-sized buffer (common operation for connections).
 */
void* bolt_pool_get_buffer(BoltMemoryPool* pool, int arena_id);

/*
 * Return a buffer to the pool.
 */
void bolt_pool_return_buffer(BoltMemoryPool* pool, int arena_id, void* buffer);

/*
 * Get pool statistics.
 */
void bolt_pool_stats(BoltMemoryPool* pool, size_t* total_alloc, size_t* total_free);

#endif /* MEMORY_POOL_H */

