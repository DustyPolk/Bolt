#ifndef BOLT_SERVER_H
#define BOLT_SERVER_H

#include "bolt.h"
#include "iocp.h"
#include "threadpool.h"
#include "connection.h"
#include "memory_pool.h"
#include "file_cache.h"

/*
 * Main Bolt server structure.
 */
struct BoltServer {
    /* Configuration */
    int port;
    const char* web_root;
    
    /* Core components */
    BoltIOCP* iocp;
    BoltThreadPool* thread_pool;
    BoltConnectionPool* conn_pool;
    BoltMemoryPool* mem_pool;
    BoltFileCache* file_cache;
    
    /* State */
    volatile bool running;
    ULONGLONG start_time;
    bool stats_enabled;
    DWORD stats_interval_ms;
    
    /* Statistics */
    volatile LONG64 total_connections;
    volatile LONG64 active_connections;
};

/*
 * Create and initialize Bolt server.
 */
BoltServer* bolt_server_create(int port);

/*
 * Run the server (blocks until shutdown).
 */
void bolt_server_run(BoltServer* server);

/*
 * Stop the server gracefully.
 */
void bolt_server_stop(BoltServer* server);

/*
 * Destroy server and free resources.
 */
void bolt_server_destroy(BoltServer* server);

/*
 * Print server statistics.
 */
void bolt_server_print_stats(BoltServer* server);

#endif /* BOLT_SERVER_H */

