#ifndef CONNECTION_H
#define CONNECTION_H

#include "bolt.h"
#include "iocp.h"
#include "http.h"

/*
 * Connection state machine for managing HTTP connections.
 */

/* Connection structure */
struct BoltConnection {
    /* Socket */
    SOCKET socket;
    BoltConnectionState state;
    
    /* Overlapped structures for async operations */
    BoltOverlapped recv_overlapped;
    BoltOverlapped send_overlapped;
    
    /* Receive buffer */
    char* recv_buffer;
    size_t recv_buffer_size;
    size_t recv_offset;
    
    /* Send buffer */
    char* send_buffer;
    size_t send_buffer_size;
    size_t send_offset;
    size_t send_remaining;
    
    /* HTTP state */
    HttpRequest request;
    bool keep_alive;
    int requests_served;
    
    /* File transfer state */
    HANDLE file_handle;
    size_t file_size;
    size_t file_offset;
    
    /* Timing */
    ULONGLONG connect_time;
    ULONGLONG last_activity;
    
    /* Pool management */
    int arena_id;
    struct BoltConnection* next_free;  /* For connection pool */
    
    /* Statistics */
    size_t bytes_received;
    size_t bytes_sent;
};

/* Connection pool */
typedef struct BoltConnectionPool {
    BoltConnection* connections;
    BoltConnection* free_list;
    int capacity;
    volatile LONG active_count;
    CRITICAL_SECTION lock;
} BoltConnectionPool;

/*
 * Create connection pool with specified capacity.
 */
BoltConnectionPool* bolt_conn_pool_create(int capacity);

/*
 * Destroy connection pool.
 */
void bolt_conn_pool_destroy(BoltConnectionPool* pool);

/*
 * Acquire a connection from the pool.
 */
BoltConnection* bolt_conn_acquire(BoltConnectionPool* pool);

/*
 * Release a connection back to the pool.
 */
void bolt_conn_release(BoltConnectionPool* pool, BoltConnection* conn);

/*
 * Initialize connection for a new accepted socket.
 */
void bolt_conn_init(BoltConnection* conn, SOCKET socket, int arena_id);

/*
 * Reset connection for keep-alive reuse.
 */
void bolt_conn_reset(BoltConnection* conn);

/*
 * Close connection and cleanup.
 */
void bolt_conn_close(BoltConnection* conn);

/*
 * Handle state transition.
 */
void bolt_conn_set_state(BoltConnection* conn, BoltConnectionState state);

/*
 * Check if connection has timed out.
 */
bool bolt_conn_is_timed_out(BoltConnection* conn, DWORD timeout_ms);

/*
 * Process received data (parse HTTP request).
 */
bool bolt_conn_process_recv(BoltConnection* conn, DWORD bytes_received);

/*
 * Handle a completed HTTP request.
 */
void bolt_conn_handle_request(BoltConnection* conn);

#endif /* CONNECTION_H */

