#include "../include/bolt_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations - made non-static for use in threadpool */
BoltRateLimiter* bolt_rate_limiter_create(void);
void bolt_rate_limiter_destroy(BoltRateLimiter* limiter);
bool bolt_rate_limiter_check(BoltRateLimiter* limiter, uint32_t ip);
void bolt_rate_limiter_increment(BoltRateLimiter* limiter, uint32_t ip);
void bolt_rate_limiter_decrement(BoltRateLimiter* limiter, uint32_t ip);

/* Simple hash function for IP addresses */
static uint32_t hash_ip(uint32_t ip) {
    return ip % BOLT_RATE_LIMIT_TABLE_SIZE;
}

/* Create rate limiter */
BoltRateLimiter* bolt_rate_limiter_create(void) {
    BoltRateLimiter* limiter = (BoltRateLimiter*)calloc(1, sizeof(BoltRateLimiter));
    if (!limiter) return NULL;
    InitializeCriticalSection(&limiter->lock);
    return limiter;
}

/* Destroy rate limiter */
void bolt_rate_limiter_destroy(BoltRateLimiter* limiter) {
    if (!limiter) return;
    
    EnterCriticalSection(&limiter->lock);
    for (int i = 0; i < BOLT_RATE_LIMIT_TABLE_SIZE; i++) {
        RateLimitEntry* entry = limiter->table[i];
        while (entry) {
            RateLimitEntry* next = entry->next;
            free(entry);
            entry = next;
        }
    }
    LeaveCriticalSection(&limiter->lock);
    DeleteCriticalSection(&limiter->lock);
    free(limiter);
}

/* Check if IP can make another connection */
bool bolt_rate_limiter_check(BoltRateLimiter* limiter, uint32_t ip) {
    if (!limiter) return true;  /* No limiter = allow all */
    
    EnterCriticalSection(&limiter->lock);
    
    uint32_t idx = hash_ip(ip);
    RateLimitEntry* entry = limiter->table[idx];
    
    /* Find existing entry */
    while (entry && entry->ip != ip) {
        entry = entry->next;
    }
    
    bool allowed = true;
    if (entry) {
        /* Check limit */
        if (entry->connection_count >= BOLT_MAX_CONNECTIONS_PER_IP) {
            allowed = false;
        }
    }
    
    LeaveCriticalSection(&limiter->lock);
    return allowed;
}

/* Increment connection count for IP */
void bolt_rate_limiter_increment(BoltRateLimiter* limiter, uint32_t ip) {
    if (!limiter) return;
    
    EnterCriticalSection(&limiter->lock);
    
    uint32_t idx = hash_ip(ip);
    RateLimitEntry* entry = limiter->table[idx];
    
    /* Find existing entry */
    while (entry && entry->ip != ip) {
        entry = entry->next;
    }
    
    if (!entry) {
        /* Create new entry */
        entry = (RateLimitEntry*)calloc(1, sizeof(RateLimitEntry));
        if (entry) {
            entry->ip = ip;
            entry->connection_count = 0;
            entry->next = limiter->table[idx];
            limiter->table[idx] = entry;
        }
    }
    
    if (entry) {
        InterlockedIncrement(&entry->connection_count);
        entry->last_seen = GetTickCount64();
    }
    
    LeaveCriticalSection(&limiter->lock);
}

/* Decrement connection count for IP */
void bolt_rate_limiter_decrement(BoltRateLimiter* limiter, uint32_t ip) {
    if (!limiter) return;
    
    EnterCriticalSection(&limiter->lock);
    
    uint32_t idx = hash_ip(ip);
    RateLimitEntry* entry = limiter->table[idx];
    
    /* Find existing entry */
    while (entry && entry->ip != ip) {
        entry = entry->next;
    }
    
    if (entry) {
        LONG count = InterlockedDecrement(&entry->connection_count);
        entry->last_seen = GetTickCount64();
        
        /* Remove entry if count reaches zero (optional cleanup) */
        if (count <= 0 && entry->connection_count <= 0) {
            /* Remove from chain */
            if (limiter->table[idx] == entry) {
                limiter->table[idx] = entry->next;
            } else {
                RateLimitEntry* prev = limiter->table[idx];
                while (prev && prev->next != entry) {
                    prev = prev->next;
                }
                if (prev) {
                    prev->next = entry->next;
                }
            }
            free(entry);
        }
    }
    
    LeaveCriticalSection(&limiter->lock);
}

/*
 * Create Bolt server.
 */
BoltServer* bolt_server_create(int port) {
    BoltServer* server = (BoltServer*)calloc(1, sizeof(BoltServer));
    if (!server) {
        BOLT_ERROR("Failed to allocate server");
        return NULL;
    }
    
    server->port = port;
    server->web_root = BOLT_WEB_ROOT;
    server->running = false;
    server->stats_enabled = false;
    server->stats_interval_ms = 1000;
    
    /* Get CPU count for thread pool sizing */
    int cpu_count = bolt_get_cpu_count();
    int num_threads = cpu_count * BOLT_THREADS_PER_CORE;
    if (num_threads < BOLT_MIN_THREADS) num_threads = BOLT_MIN_THREADS;
    if (num_threads > BOLT_MAX_THREADS) num_threads = BOLT_MAX_THREADS;
    
    printf("\n");
    printf("  ⚡ BOLT - High Performance HTTP Server\n");
    printf("  ==========================================\n");
    printf("  Version:    %s\n", BOLT_VERSION_STRING);
    printf("  CPU Cores:  %d\n", cpu_count);
    printf("  Threads:    %d\n", num_threads);
    printf("  ==========================================\n\n");
    
    /* Create memory pool */
    printf("  [1/4] Creating memory pool...\n");
    server->mem_pool = bolt_pool_create(num_threads, BOLT_POOL_BLOCK_SIZE);
    if (!server->mem_pool) {
        BOLT_ERROR("Failed to create memory pool");
        free(server);
        return NULL;
    }
    
    /* Create connection pool */
    printf("  [2/4] Creating connection pool (%d connections)...\n", BOLT_MAX_CONNECTIONS);
    server->conn_pool = bolt_conn_pool_create(BOLT_MAX_CONNECTIONS);
    if (!server->conn_pool) {
        BOLT_ERROR("Failed to create connection pool");
        bolt_pool_destroy(server->mem_pool);
        free(server);
        return NULL;
    }
    
    /* Create small file cache */
#if BOLT_ENABLE_FILE_CACHE
    server->file_cache = bolt_file_cache_create(BOLT_FILE_CACHE_CAPACITY, BOLT_FILE_CACHE_MAX_TOTAL_BYTES);
#endif

    /* Create rate limiter */
    server->rate_limiter = bolt_rate_limiter_create();
    if (!server->rate_limiter) {
        BOLT_ERROR("Failed to create rate limiter");
        if (server->file_cache) bolt_file_cache_destroy(server->file_cache);
        bolt_conn_pool_destroy(server->conn_pool);
        bolt_pool_destroy(server->mem_pool);
        free(server);
        return NULL;
    }

    /* Create IOCP */
    printf("  [3/4] Initializing IOCP on port %d...\n", port);
    server->iocp = bolt_iocp_create(port, num_threads * 2);
    if (!server->iocp) {
        BOLT_ERROR("Failed to create IOCP");
        bolt_conn_pool_destroy(server->conn_pool);
        bolt_pool_destroy(server->mem_pool);
        free(server);
        return NULL;
    }
    
    /* Set global reference before creating thread pool */
    g_bolt_server = server;
    
    /* Create thread pool */
    printf("  [4/4] Starting %d worker threads...\n", num_threads);
    server->thread_pool = bolt_threadpool_create(server->iocp->handle, num_threads);
    if (!server->thread_pool) {
        BOLT_ERROR("Failed to create thread pool");
        g_bolt_server = NULL;
        bolt_iocp_destroy(server->iocp);
        bolt_conn_pool_destroy(server->conn_pool);
        bolt_pool_destroy(server->mem_pool);
        free(server);
        return NULL;
    }
    
    server->start_time = GetTickCount64();
    server->running = true;
    
    return server;
}

/*
 * Run the server.
 */
void bolt_server_run(BoltServer* server) {
    if (!server) return;
    
    printf("\n");
    printf("  ==========================================\n");
    printf("  ⚡ Bolt is running!\n");
    printf("  ==========================================\n");
    printf("  Web Root:   ./%s/\n", server->web_root);
    printf("  Port:       %d\n", server->port);
    printf("  URL:        http://localhost:%d/\n", server->port);
    printf("  ==========================================\n");
    printf("  Press Ctrl+C to stop\n");
    printf("  ==========================================\n\n");
    
    /* Main thread can do periodic tasks while workers handle requests */
    while (server->running) {
        Sleep(server->stats_interval_ms);
        if (server->stats_enabled) {
            bolt_server_print_stats(server);
        }
    }
}

/*
 * Stop the server.
 */
void bolt_server_stop(BoltServer* server) {
    if (server) {
        printf("\n  Shutting down Bolt...\n");
        server->running = false;
        
        if (server->iocp) {
            server->iocp->running = false;
        }
    }
}

/*
 * Destroy server.
 */
void bolt_server_destroy(BoltServer* server) {
    if (!server) return;
    
    /* Stop accepting new connections */
    server->running = false;
    
    /* Destroy in reverse order of creation */
    printf("  Stopping worker threads...\n");
    if (server->thread_pool) {
        bolt_threadpool_destroy(server->thread_pool);
    }
    
    printf("  Closing IOCP...\n");
    if (server->iocp) {
        bolt_iocp_destroy(server->iocp);
    }
    
    printf("  Releasing connections...\n");
    if (server->conn_pool) {
        bolt_conn_pool_destroy(server->conn_pool);
    }
    
    printf("  Freeing memory pool...\n");
    if (server->mem_pool) {
        bolt_pool_destroy(server->mem_pool);
    }

    if (server->file_cache) {
        bolt_file_cache_destroy(server->file_cache);
        server->file_cache = NULL;
    }
    
    if (server->rate_limiter) {
        bolt_rate_limiter_destroy(server->rate_limiter);
    }
    
    g_bolt_server = NULL;
    free(server);
    
    printf("  Bolt stopped.\n\n");
}

/*
 * Print server statistics.
 */
void bolt_server_print_stats(BoltServer* server) {
    if (!server) return;
    
    LONG64 total_requests = 0;
    LONG64 bytes_sent = 0;
    LONG64 bytes_received = 0;
    
    bolt_threadpool_stats(server->thread_pool, &total_requests, &bytes_sent, &bytes_received);
    
    ULONGLONG uptime = (GetTickCount64() - server->start_time) / 1000;
    double rps = uptime > 0 ? (double)total_requests / uptime : 0;
    
    printf("\n  === Bolt Statistics ===\n");
    printf("  Uptime:     %llu seconds\n", uptime);
    printf("  Requests:   %lld (%.1f req/sec)\n", total_requests, rps);
    printf("  Sent:       %.2f MB\n", bytes_sent / (1024.0 * 1024.0));
    printf("  Received:   %.2f MB\n", bytes_received / (1024.0 * 1024.0));
    printf("  Active:     %ld connections\n", server->conn_pool->active_count);
    printf("  =========================\n");
}

