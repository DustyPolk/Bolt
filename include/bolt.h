#ifndef BOLT_H
#define BOLT_H

/*
 * ⚡ BOLT - High-Performance HTTP Server
 * 
 * A blazingly fast static file server using:
 * - Windows IOCP for async I/O
 * - Thread pool for parallel request handling
 * - TransmitFile for zero-copy file transfers
 * - HTTP Keep-Alive for connection reuse
 * - Memory pooling for allocation efficiency
 */

/* Must include winsock2.h before windows.h */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

/* Version */
#define BOLT_VERSION_MAJOR  1
#define BOLT_VERSION_MINOR  0
#define BOLT_VERSION_STRING "1.0.0"
#define BOLT_SERVER_NAME    "Bolt/" BOLT_VERSION_STRING

/*============================================================================
 * Server Configuration
 *============================================================================*/

/* Network */
#define BOLT_DEFAULT_PORT       8080
#define BOLT_BACKLOG            1024
#define BOLT_MAX_CONNECTIONS    10000

/* Timeouts (milliseconds) */
#define BOLT_ACCEPT_TIMEOUT     0           /* No timeout for accept */
#define BOLT_RECV_TIMEOUT       30000       /* 30 seconds */
#define BOLT_SEND_TIMEOUT       30000       /* 30 seconds */
#define BOLT_KEEPALIVE_TIMEOUT  60000       /* 60 seconds idle */

/* Buffers */
#define BOLT_RECV_BUFFER_SIZE   8192        /* 8 KB receive buffer */
#define BOLT_SEND_BUFFER_SIZE   65536       /* 64 KB send buffer */
#define BOLT_MAX_REQUEST_SIZE   16384       /* 16 KB max request */
#define BOLT_MAX_URI_LENGTH     2048
#define BOLT_MAX_PATH_LENGTH    512
#define BOLT_MAX_HEADER_SIZE    4096

/* AcceptEx buffer: optional initial recv + local/remote sockaddr storage */
#define BOLT_ACCEPT_RECV_BYTES  1024
#define BOLT_ACCEPT_BUFFER_SIZE (BOLT_ACCEPT_RECV_BYTES + (2 * (sizeof(struct sockaddr_in) + 16)))

/* Directory listing (disabled for max performance) */
#ifndef BOLT_ENABLE_DIR_LISTING
#define BOLT_ENABLE_DIR_LISTING 0
#endif

/* Small file cache (mixed-site acceleration) */
#ifndef BOLT_ENABLE_FILE_CACHE
#define BOLT_ENABLE_FILE_CACHE 1
#endif
#define BOLT_FILE_CACHE_MAX_ENTRY_SIZE  (48 * 1024)       /* headers+body must fit in send buffer */
#define BOLT_FILE_CACHE_MAX_TOTAL_BYTES (64 * 1024 * 1024)
#define BOLT_FILE_CACHE_CAPACITY        2048

/* Thread Pool */
#define BOLT_MIN_THREADS        2
#define BOLT_MAX_THREADS        64
#define BOLT_THREADS_PER_CORE   2           /* 2 threads per CPU core */

/* File Serving */
#define BOLT_WEB_ROOT           "public"
#define BOLT_INDEX_FILE         "index.html"
#define BOLT_MAX_FILE_SIZE      (100 * 1024 * 1024)  /* 100 MB */

/* Memory Pool */
#define BOLT_POOL_BLOCK_SIZE    4096        /* 4 KB blocks */
#define BOLT_POOL_INITIAL_BLOCKS 256        /* Pre-allocate 1 MB */

/* Keep-Alive */
#define BOLT_MAX_KEEPALIVE_REQUESTS 1000    /* Max requests per connection */

/*============================================================================
 * IOCP Operation Types
 *============================================================================*/

typedef enum {
    BOLT_OP_ACCEPT,
    BOLT_OP_RECV,
    BOLT_OP_SEND,
    BOLT_OP_TRANSMIT_FILE,
    BOLT_OP_DISCONNECT
} BoltOperationType;

/*============================================================================
 * Connection States
 *============================================================================*/

typedef enum {
    BOLT_CONN_ACCEPTING,
    BOLT_CONN_READING,
    BOLT_CONN_PROCESSING,
    BOLT_CONN_SENDING,
    BOLT_CONN_SENDING_FILE,
    BOLT_CONN_KEEPALIVE,
    BOLT_CONN_CLOSING,
    BOLT_CONN_CLOSED
} BoltConnectionState;

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct BoltServer BoltServer;
typedef struct BoltConnection BoltConnection;
typedef struct BoltMemoryPool BoltMemoryPool;
typedef struct BoltThreadPool BoltThreadPool;

/*============================================================================
 * Global Server Instance
 *============================================================================*/

extern BoltServer* g_bolt_server;

/*============================================================================
 * Utility Macros
 *============================================================================*/

#define BOLT_ALIGN(size, alignment) (((size) + (alignment) - 1) & ~((alignment) - 1))
#define BOLT_CACHE_LINE_SIZE 64
#define BOLT_CACHE_ALIGN(size) BOLT_ALIGN(size, BOLT_CACHE_LINE_SIZE)

/* Logging */
#ifdef BOLT_DEBUG
    #define BOLT_LOG(fmt, ...) printf("[BOLT] " fmt "\n", ##__VA_ARGS__)
    #define BOLT_ERROR(fmt, ...) fprintf(stderr, "[BOLT ERROR] " fmt "\n", ##__VA_ARGS__)
#else
    #define BOLT_LOG(fmt, ...) ((void)0)
    #define BOLT_ERROR(fmt, ...) fprintf(stderr, "[BOLT ERROR] " fmt "\n", ##__VA_ARGS__)
#endif

/* Suppress unused parameter warnings */
#define BOLT_UNUSED(x) (void)(x)

#endif /* BOLT_H */

