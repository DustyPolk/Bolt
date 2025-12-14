#include "../include/metrics.h"
#include "../include/threadpool.h"
#include <stdio.h>
#include <string.h>

/*
 * Check if URI is a metrics endpoint.
 */
bool metrics_is_endpoint(const char* uri) {
    if (!uri) return false;
    return (strcmp(uri, "/metrics") == 0 || strcmp(uri, "/stats") == 0);
}

/*
 * Generate JSON metrics response.
 */
bool metrics_generate_json(BoltServer* server, char* buffer, size_t buffer_size, size_t* out_len) {
    if (!server || !buffer || buffer_size == 0) return false;
    
    LONG64 total_requests = 0;
    LONG64 bytes_sent = 0;
    LONG64 bytes_received = 0;
    
    bolt_threadpool_stats(server->thread_pool, &total_requests, &bytes_sent, &bytes_received);
    
    ULONGLONG uptime = (GetTickCount64() - server->start_time) / 1000;
    double rps = uptime > 0 ? (double)total_requests / uptime : 0;
    
    int len = snprintf(buffer, buffer_size,
        "{\n"
        "  \"server\": {\n"
        "    \"name\": \"" BOLT_SERVER_NAME "\",\n"
        "    \"version\": \"" BOLT_VERSION_STRING "\",\n"
        "    \"uptime_seconds\": %llu\n"
        "  },\n"
        "  \"requests\": {\n"
        "    \"total\": %lld,\n"
        "    \"requests_per_second\": %.2f\n"
        "  },\n"
        "  \"connections\": {\n"
        "    \"active\": %ld,\n"
        "    \"max\": %d\n"
        "  },\n"
        "  \"bandwidth\": {\n"
        "    \"bytes_sent\": %lld,\n"
        "    \"bytes_received\": %lld,\n"
        "    \"bytes_sent_mb\": %.2f,\n"
        "    \"bytes_received_mb\": %.2f\n"
        "  },\n"
        "  \"cache\": {\n"
        "    \"enabled\": %s\n"
        "  }\n"
        "}\n",
        uptime,
        total_requests,
        rps,
        server->conn_pool ? server->conn_pool->active_count : 0,
        server->conn_pool ? server->conn_pool->capacity : 0,
        bytes_sent,
        bytes_received,
        bytes_sent / (1024.0 * 1024.0),
        bytes_received / (1024.0 * 1024.0),
        server->file_cache ? "true" : "false"
    );
    
    if (len < 0 || len >= (int)buffer_size) {
        return false;
    }
    
    if (out_len) {
        *out_len = (size_t)len;
    }
    
    return true;
}

