#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "bolt.h"

/*
 * High-performance thread pool using IOCP as the work queue.
 * Workers pull completions directly from the IOCP.
 */

/* Worker thread info */
typedef struct BoltWorker {
    HANDLE thread;
    DWORD thread_id;
    int worker_id;
    volatile bool running;
    
    /* Statistics */
    volatile LONG64 requests_handled;
    volatile LONG64 bytes_sent;
    volatile LONG64 bytes_received;
} BoltWorker;

/* Thread pool */
struct BoltThreadPool {
    BoltWorker* workers;
    int num_workers;
    HANDLE iocp;  /* Shared IOCP handle */
    volatile bool shutdown;
    
    /* Pool statistics */
    volatile LONG64 total_requests;
};

/*
 * Create thread pool with specified number of workers.
 * iocp is the completion port workers will wait on.
 */
BoltThreadPool* bolt_threadpool_create(HANDLE iocp, int num_workers);

/*
 * Shutdown and destroy the thread pool.
 * Waits for all workers to finish.
 */
void bolt_threadpool_destroy(BoltThreadPool* pool);

/*
 * Get the number of CPU cores (for sizing the pool).
 */
int bolt_get_cpu_count(void);

/*
 * Get worker statistics.
 */
void bolt_threadpool_stats(BoltThreadPool* pool, 
                           LONG64* total_requests,
                           LONG64* bytes_sent,
                           LONG64* bytes_received);

#endif /* THREADPOOL_H */

