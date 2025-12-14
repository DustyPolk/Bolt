#include "../include/bolt_server.h"
#include "../include/config.h"
#include "../include/logger.h"
#include "../include/vhost.h"
#include "../include/rewrite.h"
#include "../include/proxy.h"
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
 * Create Bolt server (backward compatibility).
 */
BoltServer* bolt_server_create(int port) {
    BoltConfig config;
    config_load_defaults(&config);
    config.port = port;
    return bolt_server_create_with_config(&config);
}

/*
 * Create Bolt server with configuration.
 */
BoltServer* bolt_server_create_with_config(const BoltConfig* config) {
    if (!config) return NULL;
    
    BoltServer* server = (BoltServer*)calloc(1, sizeof(BoltServer));
    if (!server) {
        BOLT_ERROR("Failed to allocate server");
        return NULL;
    }
    
    server->port = config->port;
    server->web_root = config->web_root[0] ? config->web_root : BOLT_WEB_ROOT;
    server->running = false;
    server->stats_enabled = false;
    server->stats_interval_ms = 1000;
    
    /* Get CPU count for thread pool sizing */
    int cpu_count = bolt_get_cpu_count();
    int num_threads = config->worker_threads > 0 ? config->worker_threads :
                      cpu_count * BOLT_THREADS_PER_CORE;
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
    printf("  [1/6] Creating memory pool...\n");
    server->mem_pool = bolt_pool_create(num_threads, BOLT_POOL_BLOCK_SIZE);
    if (!server->mem_pool) {
        BOLT_ERROR("Failed to create memory pool");
        free(server);
        return NULL;
    }
    
    /* Create connection pool */
    int max_conns = config->max_connections > 0 ? config->max_connections : BOLT_MAX_CONNECTIONS;
    printf("  [2/6] Creating connection pool (%d connections)...\n", max_conns);
    server->conn_pool = bolt_conn_pool_create(max_conns);
    if (!server->conn_pool) {
        BOLT_ERROR("Failed to create connection pool");
        bolt_pool_destroy(server->mem_pool);
        free(server);
        return NULL;
    }
    
    /* Create small file cache */
    if (config->enable_file_cache) {
        server->file_cache = bolt_file_cache_create(BOLT_FILE_CACHE_CAPACITY, BOLT_FILE_CACHE_MAX_TOTAL_BYTES);
    }

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
    
    /* Create virtual host manager */
    server->vhost_manager = vhost_manager_create();
    if (!server->vhost_manager) {
        BOLT_ERROR("Failed to create virtual host manager");
        bolt_rate_limiter_destroy(server->rate_limiter);
        if (server->file_cache) bolt_file_cache_destroy(server->file_cache);
        bolt_conn_pool_destroy(server->conn_pool);
        bolt_pool_destroy(server->mem_pool);
        free(server);
        return NULL;
    }
    
    /* Create rewrite engine */
    server->rewrite_engine = rewrite_engine_create();
    if (!server->rewrite_engine) {
        BOLT_ERROR("Failed to create rewrite engine");
        vhost_manager_destroy(server->vhost_manager);
        bolt_rate_limiter_destroy(server->rate_limiter);
        if (server->file_cache) bolt_file_cache_destroy(server->file_cache);
        bolt_conn_pool_destroy(server->conn_pool);
        bolt_pool_destroy(server->mem_pool);
        free(server);
        return NULL;
    }
    
    /* Create proxy config */
    server->proxy_config = proxy_config_create();
    if (!server->proxy_config) {
        BOLT_ERROR("Failed to create proxy config");
        rewrite_engine_destroy(server->rewrite_engine);
        vhost_manager_destroy(server->vhost_manager);
        bolt_rate_limiter_destroy(server->rate_limiter);
        if (server->file_cache) bolt_file_cache_destroy(server->file_cache);
        bolt_conn_pool_destroy(server->conn_pool);
        bolt_pool_destroy(server->mem_pool);
        free(server);
        return NULL;
    }
    
    /* Create logger */
    BoltLogLevel log_level = (BoltLogLevel)config->log_level;
    server->logger = logger_create(config->access_log_path, config->error_log_path, log_level);
    if (!server->logger) {
        BOLT_ERROR("Failed to create logger");
        proxy_config_destroy(server->proxy_config);
        rewrite_engine_destroy(server->rewrite_engine);
        vhost_manager_destroy(server->vhost_manager);
        bolt_rate_limiter_destroy(server->rate_limiter);
        if (server->file_cache) bolt_file_cache_destroy(server->file_cache);
        bolt_conn_pool_destroy(server->conn_pool);
        bolt_pool_destroy(server->mem_pool);
        free(server);
        return NULL;
    }

    /* Create IOCP */
    printf("  [4/6] Initializing IOCP on port %d...\n", config->port);
    server->iocp = bolt_iocp_create(config->port, num_threads * 2);
    if (!server->iocp) {
        BOLT_ERROR("Failed to create IOCP");
        logger_destroy(server->logger);
        proxy_config_destroy(server->proxy_config);
        rewrite_engine_destroy(server->rewrite_engine);
        vhost_manager_destroy(server->vhost_manager);
        bolt_rate_limiter_destroy(server->rate_limiter);
        if (server->file_cache) bolt_file_cache_destroy(server->file_cache);
        bolt_conn_pool_destroy(server->conn_pool);
        bolt_pool_destroy(server->mem_pool);
        free(server);
        return NULL;
    }
    
    /* Set global reference before creating thread pool */
    g_bolt_server = server;
    
    /* Create thread pool */
    printf("  [6/6] Starting %d worker threads...\n", num_threads);
    server->thread_pool = bolt_threadpool_create(server->iocp->handle, num_threads);
    if (!server->thread_pool) {
        BOLT_ERROR("Failed to create thread pool");
        logger_destroy(server->logger);
        proxy_config_destroy(server->proxy_config);
        rewrite_engine_destroy(server->rewrite_engine);
        vhost_manager_destroy(server->vhost_manager);
        g_bolt_server = NULL;
        bolt_iocp_destroy(server->iocp);
        bolt_rate_limiter_destroy(server->rate_limiter);
        if (server->file_cache) bolt_file_cache_destroy(server->file_cache);
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
    
    if (server->logger) {
        logger_destroy(server->logger);
    }
    
    if (server->vhost_manager) {
        vhost_manager_destroy(server->vhost_manager);
    }
    
    if (server->rewrite_engine) {
        rewrite_engine_destroy(server->rewrite_engine);
    }
    
    if (server->proxy_config) {
        proxy_config_destroy(server->proxy_config);
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

