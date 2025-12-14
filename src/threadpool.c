#include "../include/threadpool.h"
#include "../include/bolt_server.h"
#include "../include/iocp.h"
#include "../include/connection.h"
#include "../include/file_server.h"
#include "../include/profiler.h"
#include "../include/bolt_server.h"  /* For rate limiter functions */
#include <stdio.h>
#include <stdlib.h>

/* Forward declarations for rate limiter */
bool bolt_rate_limiter_check(BoltRateLimiter* limiter, uint32_t ip);
void bolt_rate_limiter_increment(BoltRateLimiter* limiter, uint32_t ip);
void bolt_rate_limiter_decrement(BoltRateLimiter* limiter, uint32_t ip);

/* Forward declaration */
static DWORD WINAPI worker_thread(LPVOID param);
static bool post_send_from_offset(BoltConnection* conn);

/* Worker context passed to thread */
typedef struct {
    BoltThreadPool* pool;
    BoltWorker* worker;
} WorkerContext;

/* Re-post an async send for remaining bytes (correct OVERLAPPED usage). */
static bool post_send_from_offset(BoltConnection* conn) {
    if (!conn) return false;
    if (conn->send_offset >= conn->send_remaining) return true;

    BoltOverlapped* ov = &conn->send_overlapped;
    memset(&ov->overlapped, 0, sizeof(OVERLAPPED));
    ov->op_type = BOLT_OP_SEND;
    ov->connection = conn;
    ov->wsa_buf.buf = conn->send_buffer + conn->send_offset;
    ov->wsa_buf.len = (ULONG)(conn->send_remaining - conn->send_offset);

    DWORD bytes_sent = 0;
    int rc = WSASend(conn->socket, &ov->wsa_buf, 1, &bytes_sent, 0, &ov->overlapped, NULL);
    if (rc == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSA_IO_PENDING) {
            return true;
        }
        return false;
    }
    return true;
}

/*
 * Get number of CPU cores.
 */
int bolt_get_cpu_count(void) {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
}

/*
 * Create thread pool.
 */
BoltThreadPool* bolt_threadpool_create(HANDLE iocp, int num_workers) {
    BoltThreadPool* pool = (BoltThreadPool*)calloc(1, sizeof(BoltThreadPool));
    if (!pool) {
        return NULL;
    }
    
    pool->iocp = iocp;
    pool->num_workers = num_workers;
    pool->shutdown = false;
    pool->total_requests = 0;
    
    /* Allocate workers */
    pool->workers = (BoltWorker*)calloc(num_workers, sizeof(BoltWorker));
    if (!pool->workers) {
        free(pool);
        return NULL;
    }
    
    /* Create worker threads */
    for (int i = 0; i < num_workers; i++) {
        pool->workers[i].worker_id = i;
        pool->workers[i].running = true;
        pool->workers[i].requests_handled = 0;
        pool->workers[i].bytes_sent = 0;
        pool->workers[i].bytes_received = 0;
        
        /* Create context (thread will free it) */
        WorkerContext* ctx = (WorkerContext*)malloc(sizeof(WorkerContext));
        if (!ctx) {
            BOLT_ERROR("Failed to allocate worker context");
            continue;
        }
        ctx->pool = pool;
        ctx->worker = &pool->workers[i];
        
        pool->workers[i].thread = CreateThread(
            NULL,
            0,
            worker_thread,
            ctx,
            0,
            &pool->workers[i].thread_id
        );
        
        if (!pool->workers[i].thread) {
            BOLT_ERROR("Failed to create worker thread %d: %lu", i, (unsigned long)GetLastError());
            free(ctx);
        }
    }
    
    return pool;
}

/*
 * Destroy thread pool.
 */
void bolt_threadpool_destroy(BoltThreadPool* pool) {
    if (!pool) return;
    
    /* Signal shutdown */
    pool->shutdown = true;
    
    /* Post completion packets to wake up workers */
    for (int i = 0; i < pool->num_workers; i++) {
        PostQueuedCompletionStatus(pool->iocp, 0, 0, NULL);
    }
    
    /* Wait for workers to finish */
    for (int i = 0; i < pool->num_workers; i++) {
        if (pool->workers[i].thread) {
            pool->workers[i].running = false;
            WaitForSingleObject(pool->workers[i].thread, 5000);
            CloseHandle(pool->workers[i].thread);
        }
    }
    
    free(pool->workers);
    free(pool);
}

/*
 * Worker thread function.
 */
static DWORD WINAPI worker_thread(LPVOID param) {
    WorkerContext* ctx = (WorkerContext*)param;
    BoltThreadPool* pool = ctx->pool;
    BoltWorker* worker = ctx->worker;
    free(ctx);  /* Free context immediately */
    
    BOLT_LOG("Worker %d started (thread %u)", worker->worker_id, worker->thread_id);
    
    while (worker->running && !pool->shutdown) {
        DWORD bytes_transferred = 0;
        ULONG_PTR completion_key = 0;
        BoltOverlapped* overlapped = NULL;
        
        /* Wait for completion */
        BOOL success = GetQueuedCompletionStatus(
            pool->iocp,
            &bytes_transferred,
            &completion_key,
            (OVERLAPPED**)&overlapped,
            1000  /* 1 second timeout for shutdown check */
        );
        
        if (!success) {
            DWORD error = GetLastError();
            if (error == WAIT_TIMEOUT) {
                continue;  /* Check shutdown and retry */
            }
            if (overlapped) {
                /* Handle error for specific operation */
                BoltConnection* conn = overlapped->connection;
                if (conn) {
                    bolt_conn_close(conn);
                }
            }
            continue;
        }
        
        /* Check for shutdown signal */
        if (!overlapped) {
            if (pool->shutdown) {
                break;
            }
            continue;
        }
        
        /* Handle completion based on operation type */
        switch (overlapped->op_type) {
            case BOLT_OP_ACCEPT: {
                BoltIOCP* iocp = g_bolt_server ? g_bolt_server->iocp : NULL;
                if (!iocp) break;

                int accept_idx = overlapped->accept_index;
                if (accept_idx >= 0 && accept_idx < iocp->num_accepts) {
                    SOCKET client_socket = iocp->accept_sockets[accept_idx];
                    
                    /* Extract client IP from AcceptEx buffer */
                    struct sockaddr_in* local_addr = NULL;
                    struct sockaddr_in* remote_addr = NULL;
                    int local_len = 0;
                    int remote_len = 0;
                    uint32_t client_ip = 0;
                    
                    if (iocp->GetAcceptExSockaddrs) {
                        iocp->GetAcceptExSockaddrs(
                            overlapped->buffer,
                            bytes_transferred,
                            sizeof(struct sockaddr_in) + 16,
                            sizeof(struct sockaddr_in) + 16,
                            (struct sockaddr**)&local_addr, &local_len,
                            (struct sockaddr**)&remote_addr, &remote_len
                        );
                        if (remote_addr) {
                            client_ip = remote_addr->sin_addr.s_addr;
                        }
                    }
                    
                    /* Rate limiting: check if IP can make another connection */
                    if (g_bolt_server->rate_limiter && client_ip != 0) {
                        if (!bolt_rate_limiter_check(g_bolt_server->rate_limiter, client_ip)) {
                            /* Rate limit exceeded - reject connection */
                            closesocket(client_socket);
                            iocp->accept_sockets[accept_idx] = INVALID_SOCKET;
                            bolt_iocp_post_accept(iocp, accept_idx);
                            break;
                        }
                    }
                    
                    /* Inherit socket options from listen socket */
                    setsockopt(client_socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                              (char*)&iocp->listen_socket, sizeof(iocp->listen_socket));
                    
                    /* Disable Nagle */
                    int opt = 1;
                    setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));
                    
                    /* Get connection from pool */
                    BoltConnection* conn = bolt_conn_acquire(g_bolt_server->conn_pool);
                    if (conn) {
                        /* Store client IP in connection */
                        conn->client_ip = client_ip;
                        
                        bolt_conn_init(conn, client_socket, worker->worker_id);
                        
                        /* Increment rate limiter counter */
                        if (g_bolt_server->rate_limiter && client_ip != 0) {
                            bolt_rate_limiter_increment(g_bolt_server->rate_limiter, client_ip);
                        }
                        
                        /* Associate with IOCP */
                        bolt_iocp_associate(iocp, client_socket, (void*)conn);

                        /* If AcceptEx received initial data, prefill recv buffer */
                        if (bytes_transferred > 0) {
                            DWORD copy_len = bytes_transferred;
                            if (copy_len > (DWORD)conn->recv_buffer_size) {
                                copy_len = (DWORD)conn->recv_buffer_size;
                            }
                            memcpy(conn->recv_buffer, overlapped->buffer, copy_len);
                            conn->recv_offset = copy_len;
                            conn->recv_buffer[conn->recv_offset < conn->recv_buffer_size ? conn->recv_offset : (conn->recv_buffer_size - 1)] = '\0';

                            if (bolt_conn_process_recv(conn, 0)) {
                                bolt_conn_handle_request(conn);
                                InterlockedIncrement64(&worker->requests_handled);
                                InterlockedIncrement64(&pool->total_requests);
                            } else {
                                bolt_iocp_post_recv(iocp, conn);
                            }
                        } else {
                            /* Post first recv */
                            bolt_iocp_post_recv(iocp, conn);
                        }

                        BOLT_LOG("Worker %d accepted connection (slot %d)", worker->worker_id, accept_idx);
                    } else {
                        closesocket(client_socket);
                    }
                    
                    /* Re-post accept */
                    iocp->accept_sockets[accept_idx] = INVALID_SOCKET;
                    bolt_iocp_post_accept(iocp, accept_idx);
                }
                break;
            }
            
            case BOLT_OP_RECV: {
                BoltConnection* conn = overlapped->connection;
                if (!conn) break;
                
                if (bytes_transferred == 0) {
                    /* Connection closed */
                    bolt_conn_close(conn);
                    bolt_conn_release(g_bolt_server->conn_pool, conn);
                } else {
                    worker->bytes_received += bytes_transferred;
                    
                    /* Process received data */
                    if (bolt_conn_process_recv(conn, bytes_transferred)) {
                        /* Request complete or timed out - handle it */
                        if (conn->request.valid) {
                            bolt_conn_handle_request(conn);
                            InterlockedIncrement64(&worker->requests_handled);
                            InterlockedIncrement64(&pool->total_requests);
                            
                            /* Log access (will be logged after response is sent) */
                            /* Note: Actual logging happens in send completion */
                        } else {
                            /* Invalid request or timeout - send error and close */
                            send_error_async(conn, HTTP_408_REQUEST_TIMEOUT);
                            bolt_conn_close(conn);
                            bolt_conn_release(g_bolt_server->conn_pool, conn);
                        }
                    } else {
                        /* Need more data - check timeout before posting another recv */
                        if (bolt_conn_is_timed_out(conn, BOLT_REQUEST_TIMEOUT)) {
                            /* Request timeout - close connection */
                            send_error_async(conn, HTTP_408_REQUEST_TIMEOUT);
                            bolt_conn_close(conn);
                            bolt_conn_release(g_bolt_server->conn_pool, conn);
                        } else {
                            /* Post another recv */
                            bolt_iocp_post_recv(g_bolt_server->iocp, conn);
                        }
                    }
                }
                break;
            }
            
            case BOLT_OP_SEND: {
                BoltConnection* conn = overlapped->connection;
                if (!conn) break;
                
                worker->bytes_sent += bytes_transferred;
                conn->bytes_sent += bytes_transferred;
                
                /* Check if more to send */
                conn->send_offset += bytes_transferred;
                if (conn->send_offset < conn->send_remaining) {
                    /* Continue sending (re-post with fresh OVERLAPPED) */
                    if (!post_send_from_offset(conn)) {
                        bolt_conn_close(conn);
                        bolt_conn_release(g_bolt_server->conn_pool, conn);
                    }
                } else {
                    /* End profiling */
                    if (g_bolt_server && g_bolt_server->logger) {
                        profiler_end_request(conn, g_bolt_server->logger);
                    }
                    
                    /* Send complete */
                    if (conn->keep_alive && conn->requests_served < BOLT_MAX_KEEPALIVE_REQUESTS) {
                        /* Reset for next request */
                        bolt_conn_reset(conn);
                        bolt_iocp_post_recv(g_bolt_server->iocp, conn);
                    } else {
                        bolt_conn_close(conn);
                        bolt_conn_release(g_bolt_server->conn_pool, conn);
                    }
                }
                break;
            }
            
            case BOLT_OP_TRANSMIT_FILE: {
                BoltConnection* conn = overlapped->connection;
                if (!conn) break;
                
                worker->bytes_sent += bytes_transferred;
                conn->bytes_sent += bytes_transferred;
                
                /* Log access entry */
                if (g_bolt_server->logger && conn->request.valid) {
                    char ip_str[64];
                    struct in_addr addr;
                    addr.s_addr = conn->client_ip;
                    inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
                    
                    const char* method_str = "UNKNOWN";
                    switch (conn->request.method) {
                        case HTTP_GET: method_str = "GET"; break;
                        case HTTP_HEAD: method_str = "HEAD"; break;
                        case HTTP_POST: method_str = "POST"; break;
                        case HTTP_OPTIONS: method_str = "OPTIONS"; break;
                        default: break;
                    }
                    
                    /* Extract Referer and User-Agent from request */
                    char referer[256] = "";
                    char user_agent[512] = "";
                    const char* req_buf = conn->recv_buffer;
                    
                    char* ref_start = strstr(req_buf, "Referer:");
                    if (ref_start) {
                        const char* ref_val = ref_start + 8;
                        while (*ref_val == ' ' || *ref_val == '\t') ref_val++;
                        const char* ref_end = ref_val;
                        while (*ref_end && *ref_end != '\r' && *ref_end != '\n') ref_end++;
                        size_t ref_len = ref_end - ref_val;
                        if (ref_len >= sizeof(referer)) ref_len = sizeof(referer) - 1;
                        strncpy(referer, ref_val, ref_len);
                        referer[ref_len] = '\0';
                    }
                    
                    char* ua_start = strstr(req_buf, "User-Agent:");
                    if (ua_start) {
                        const char* ua_val = ua_start + 11;
                        while (*ua_val == ' ' || *ua_val == '\t') ua_val++;
                        const char* ua_end = ua_val;
                        while (*ua_end && *ua_end != '\r' && *ua_end != '\n') ua_end++;
                        size_t ua_len = ua_end - ua_val;
                        if (ua_len >= sizeof(user_agent)) ua_len = sizeof(user_agent) - 1;
                        strncpy(user_agent, ua_val, ua_len);
                        user_agent[ua_len] = '\0';
                    }
                    
                    int status = 200;  /* TODO: Track actual status code */
                    logger_access(g_bolt_server->logger, ip_str, method_str,
                                 conn->request.uri, status, bytes_transferred,
                                 referer[0] ? referer : NULL,
                                 user_agent[0] ? user_agent : NULL);
                }
                
                /* Close file handle */
                if (conn->file_handle != INVALID_HANDLE_VALUE) {
                    CloseHandle(conn->file_handle);
                    conn->file_handle = INVALID_HANDLE_VALUE;
                }
                
                /* End profiling */
                if (g_bolt_server && g_bolt_server->logger) {
                    profiler_end_request(conn, g_bolt_server->logger);
                }
                
                /* Handle keep-alive or close */
                if (conn->keep_alive && conn->requests_served < BOLT_MAX_KEEPALIVE_REQUESTS) {
                    bolt_conn_reset(conn);
                    bolt_iocp_post_recv(g_bolt_server->iocp, conn);
                } else {
                    bolt_conn_close(conn);
                    bolt_conn_release(g_bolt_server->conn_pool, conn);
                }
                break;
            }
            
            case BOLT_OP_DISCONNECT: {
                BoltConnection* conn = overlapped->connection;
                if (conn) {
                    bolt_conn_release(g_bolt_server->conn_pool, conn);
                }
                break;
            }
        }
    }
    
    BOLT_LOG("Worker %d stopped", worker->worker_id);
    return 0;
}

/*
 * Get thread pool statistics.
 */
void bolt_threadpool_stats(BoltThreadPool* pool,
                           LONG64* total_requests,
                           LONG64* bytes_sent,
                           LONG64* bytes_received) {
    if (!pool) return;
    
    LONG64 sent = 0, recv = 0;
    
    for (int i = 0; i < pool->num_workers; i++) {
        sent += pool->workers[i].bytes_sent;
        recv += pool->workers[i].bytes_received;
    }
    
    if (total_requests) *total_requests = pool->total_requests;
    if (bytes_sent) *bytes_sent = sent;
    if (bytes_received) *bytes_received = recv;
}

