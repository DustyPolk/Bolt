#ifndef HTTP2_H
#define HTTP2_H

#include "bolt.h"
#include <stdbool.h>

/*
 * HTTP/2 support (placeholder - Windows HTTP/2 API integration).
 * TODO: Implement full HTTP/2 using Windows HTTP/2 API.
 */
typedef struct BoltHTTP2Context {
    bool enabled;
    /* TODO: Add HTTP/2 API handles */
} BoltHTTP2Context;

/*
 * Initialize HTTP/2 support.
 */
bool http2_init(void);

/*
 * Check if connection should use HTTP/2.
 */
bool http2_should_upgrade(const char* request_headers);

/*
 * Handle HTTP/2 connection (placeholder).
 */
bool http2_handle_connection(SOCKET socket);

#endif /* HTTP2_H */

