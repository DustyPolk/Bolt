#ifndef PROXY_H
#define PROXY_H

#include "bolt.h"
#include "http.h"
#include "connection.h"
#include <stdbool.h>

/*
 * Upstream server configuration.
 */
typedef struct BoltUpstream {
    char host[256];
    int port;
    struct BoltUpstream* next;
} BoltUpstream;

/*
 * Proxy configuration.
 */
typedef struct BoltProxyConfig {
    BoltUpstream* upstreams;
    bool enabled;
} BoltProxyConfig;

/*
 * Create proxy configuration.
 */
BoltProxyConfig* proxy_config_create(void);

/*
 * Destroy proxy configuration.
 */
void proxy_config_destroy(BoltProxyConfig* config);

/*
 * Add upstream server.
 */
bool proxy_add_upstream(BoltProxyConfig* config, const char* host, int port);

/*
 * Check if URI should be proxied.
 */
bool proxy_should_proxy(const char* uri);

/*
 * Forward request to upstream (simplified - returns false for now).
 * TODO: Implement full proxy forwarding with connection pooling.
 */
bool proxy_forward_request(BoltConnection* conn, const HttpRequest* request,
                          BoltProxyConfig* config);

#endif /* PROXY_H */

