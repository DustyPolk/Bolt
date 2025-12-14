#include "../include/connection.h"
#include "../include/bolt.h"
#include "../include/bolt_server.h"
#include "../include/iocp.h"
#include "../include/file_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global server reference */
BoltServer* g_bolt_server = NULL;

/*
 * Create connection pool.
 */
BoltConnectionPool* bolt_conn_pool_create(int capacity) {
    BoltConnectionPool* pool = (BoltConnectionPool*)calloc(1, sizeof(BoltConnectionPool));
    if (!pool) return NULL;
    
    pool->capacity = capacity;
    pool->active_count = 0;
    InitializeCriticalSection(&pool->lock);
    
    /* Pre-allocate connections */
    pool->connections = (BoltConnection*)calloc(capacity, sizeof(BoltConnection));
    if (!pool->connections) {
        DeleteCriticalSection(&pool->lock);
        free(pool);
        return NULL;
    }
    
    /* Build free list */
    pool->free_list = NULL;
    for (int i = capacity - 1; i >= 0; i--) {
        pool->connections[i].socket = INVALID_SOCKET;
        pool->connections[i].state = BOLT_CONN_CLOSED;
        pool->connections[i].file_handle = INVALID_HANDLE_VALUE;
        pool->connections[i].next_free = pool->free_list;
        pool->free_list = &pool->connections[i];
        
        /* Allocate buffers */
        pool->connections[i].recv_buffer = (char*)_aligned_malloc(BOLT_RECV_BUFFER_SIZE, 64);
        pool->connections[i].recv_buffer_size = BOLT_RECV_BUFFER_SIZE;
        pool->connections[i].send_buffer = (char*)_aligned_malloc(BOLT_SEND_BUFFER_SIZE, 64);
        pool->connections[i].send_buffer_size = BOLT_SEND_BUFFER_SIZE;
    }
    
    return pool;
}

/*
 * Destroy connection pool.
 */
void bolt_conn_pool_destroy(BoltConnectionPool* pool) {
    if (!pool) return;
    
    /* Close any active connections */
    for (int i = 0; i < pool->capacity; i++) {
        if (pool->connections[i].socket != INVALID_SOCKET) {
            closesocket(pool->connections[i].socket);
        }
        if (pool->connections[i].file_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(pool->connections[i].file_handle);
        }
        _aligned_free(pool->connections[i].recv_buffer);
        _aligned_free(pool->connections[i].send_buffer);
    }
    
    DeleteCriticalSection(&pool->lock);
    free(pool->connections);
    free(pool);
}

/*
 * Acquire connection from pool.
 */
BoltConnection* bolt_conn_acquire(BoltConnectionPool* pool) {
    if (!pool) return NULL;
    
    EnterCriticalSection(&pool->lock);
    
    BoltConnection* conn = pool->free_list;
    if (conn) {
        pool->free_list = conn->next_free;
        InterlockedIncrement(&pool->active_count);
    }
    
    LeaveCriticalSection(&pool->lock);
    
    return conn;
}

/*
 * Release connection back to pool.
 */
void bolt_conn_release(BoltConnectionPool* pool, BoltConnection* conn) {
    if (!pool || !conn) return;
    
    /* Ensure connection is closed */
    bolt_conn_close(conn);
    
    EnterCriticalSection(&pool->lock);
    
    conn->next_free = pool->free_list;
    pool->free_list = conn;
    InterlockedDecrement(&pool->active_count);
    
    LeaveCriticalSection(&pool->lock);
}

/*
 * Initialize connection for new socket.
 */
void bolt_conn_init(BoltConnection* conn, SOCKET socket, int arena_id) {
    if (!conn) return;
    
    conn->socket = socket;
    conn->state = BOLT_CONN_READING;
    conn->arena_id = arena_id;
    
    /* Reset buffers */
    conn->recv_offset = 0;
    conn->send_offset = 0;
    conn->send_remaining = 0;
    
    /* Reset HTTP state */
    memset(&conn->request, 0, sizeof(conn->request));
    conn->keep_alive = true;  /* Default to keep-alive */
    conn->requests_served = 0;
    
    /* Reset file state */
    conn->file_handle = INVALID_HANDLE_VALUE;
    conn->file_size = 0;
    conn->file_offset = 0;
    
    /* Timing */
    conn->connect_time = GetTickCount64();
    conn->last_activity = conn->connect_time;
    
    /* Statistics */
    conn->bytes_received = 0;
    conn->bytes_sent = 0;
    
    /* Setup overlapped structures */
    memset(&conn->recv_overlapped, 0, sizeof(conn->recv_overlapped));
    memset(&conn->send_overlapped, 0, sizeof(conn->send_overlapped));
    conn->recv_overlapped.connection = conn;
    conn->send_overlapped.connection = conn;
}

/*
 * Reset connection for keep-alive reuse.
 */
void bolt_conn_reset(BoltConnection* conn) {
    if (!conn) return;
    
    conn->state = BOLT_CONN_READING;
    conn->recv_offset = 0;
    conn->send_offset = 0;
    conn->send_remaining = 0;
    
    memset(&conn->request, 0, sizeof(conn->request));
    
    if (conn->file_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(conn->file_handle);
        conn->file_handle = INVALID_HANDLE_VALUE;
    }
    conn->file_size = 0;
    conn->file_offset = 0;
    
    conn->last_activity = GetTickCount64();
}

/*
 * Close connection.
 */
void bolt_conn_close(BoltConnection* conn) {
    if (!conn) return;
    
    conn->state = BOLT_CONN_CLOSED;
    
    if (conn->socket != INVALID_SOCKET) {
        /* Graceful shutdown */
        shutdown(conn->socket, SD_BOTH);
        closesocket(conn->socket);
        conn->socket = INVALID_SOCKET;
    }
    
    if (conn->file_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(conn->file_handle);
        conn->file_handle = INVALID_HANDLE_VALUE;
    }
}

/*
 * Set connection state.
 */
void bolt_conn_set_state(BoltConnection* conn, BoltConnectionState state) {
    if (conn) {
        conn->state = state;
        conn->last_activity = GetTickCount64();
    }
}

/*
 * Check if connection timed out.
 */
bool bolt_conn_is_timed_out(BoltConnection* conn, DWORD timeout_ms) {
    if (!conn) return true;
    
    ULONGLONG now = GetTickCount64();
    return (now - conn->last_activity) > timeout_ms;
}

/*
 * Process received data.
 */
bool bolt_conn_process_recv(BoltConnection* conn, DWORD bytes_received) {
    if (!conn) return false;
    
    if (bytes_received > 0) {
        conn->recv_offset += bytes_received;
        conn->bytes_received += bytes_received;
    }
    conn->last_activity = GetTickCount64();
    
    /* Null terminate for string operations */
    if (conn->recv_offset < conn->recv_buffer_size) {
        conn->recv_buffer[conn->recv_offset] = '\0';
    }
    
    /* Check for complete request (end of headers) */
    char* end_of_headers = strstr(conn->recv_buffer, "\r\n\r\n");
    if (end_of_headers) {
        /* Parse the request */
        conn->request = http_parse_request(conn->recv_buffer, conn->recv_offset);
        
        /* Check Connection header for keep-alive */
        if (conn->request.valid) {
            /* HTTP/1.1 defaults to keep-alive */
            conn->keep_alive = true;
            
            /* Look for Connection: close */
            char* conn_header = strstr(conn->recv_buffer, "Connection:");
            if (conn_header) {
                if (strstr(conn_header, "close")) {
                    conn->keep_alive = false;
                }
            }
        }
        
        return true;  /* Request complete */
    }
    
    /* Check for buffer overflow */
    if (conn->recv_offset >= BOLT_MAX_REQUEST_SIZE) {
        conn->request.valid = false;
        return true;  /* Return true to trigger error handling */
    }
    
    return false;  /* Need more data */
}

/*
 * Handle completed HTTP request.
 */
void bolt_conn_handle_request(BoltConnection* conn) {
    if (!conn) return;
    
    conn->state = BOLT_CONN_PROCESSING;
    conn->requests_served++;
    
    /* Log request */
    printf("[%s] %s\n",
           conn->request.method == HTTP_GET ? "GET" :
           conn->request.method == HTTP_HEAD ? "HEAD" : "???",
           conn->request.uri);
    
    /* Handle the request using Bolt async file server */
    bolt_file_server_handle(conn, &conn->request);
}

